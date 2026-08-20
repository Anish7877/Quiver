#include "image_manager.hpp"
#include "utils.hpp"
#include "serialization.hpp"
#include <cpr/cpr.h>
#include <blake3.h>
#include <exception>
#include <filesystem>
#include <format>
#include <chrono>
#include <fstream>
#include <thread>
#include <array>
#include <iomanip>
#include <sstream>
#include <regex>
#include<iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <algorithm>

static constexpr std::size_t  MAX_TOKEN_RESPONSE_SIZE {   64 * 1024};
static constexpr std::size_t  MAX_MANIFEST_SIZE       {    1 * 1024 * 1024};
static constexpr std::size_t  MAX_CONFIG_SIZE         {    1 * 1024 * 1024};

static constexpr std::int32_t TIMEOUT_AUTH_MS         {   30'000};
static constexpr std::int32_t TIMEOUT_MANIFEST_MS     {   30'000};
static constexpr std::int32_t TIMEOUT_CONFIG_MS       {   30'000};
static constexpr std::int32_t TIMEOUT_LAYER_MS        {  600'000};
[[nodiscard]] static auto blake3_hex(std::string_view data) -> std::string {
        blake3_hasher hasher{};
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, data.data(), data.size());
        std::array<uint8_t, BLAKE3_OUT_LEN> out{};
        blake3_hasher_finalize(&hasher, out.data(), out.size());
        std::ostringstream oss{};
        oss << std::hex << std::setfill('0');
        for (const auto byte : out) {
                oss << std::setw(2) << static_cast<unsigned>(byte);
        }
        return oss.str();
}

static auto secure_zero(std::string& s) -> void {
        if (!s.empty()) {
                volatile char* p{s.data()};
                for (std::size_t i{0}; i < s.size(); ++i) p[i] = '\0';
                s.clear();
        }
}

[[nodiscard]] static auto is_valid_repo(const std::string& repo) -> bool {
        static const std::regex valid{R"(^[a-z0-9_\-\.]+(\/[a-z0-9_\-\.]+)*$)"};
        return std::regex_match(repo, valid);
}

[[nodiscard]] static auto is_valid_tag(const std::string& tag) -> bool {
        static const std::regex valid{R"(^[a-zA-Z0-9_\-\.]{1,128}$)"};
        return std::regex_match(tag, valid);
}

[[nodiscard]] static auto is_valid_digest(const std::string& digest) -> bool {
        static const std::regex valid{R"(^(sha256|blake3):[a-f0-9]{64}$)"};
        return std::regex_match(digest, valid);
}

[[nodiscard]] static auto json_get_string(const json& obj, std::string_view field, std::string& out, std::string& error) -> bool {
        const std::string key{field};
        if (!obj.contains(key) || obj[key].is_null() || !obj[key].is_string()) [[unlikely]] {
                error = std::format("JSON Error: field '{}' is missing or not a string", field);
                return false;
        }
        out = obj[key].get<std::string>();
        return true;
}

[[nodiscard]] static auto json_get_size(const json& obj, std::string_view field, std::size_t& out, std::string& error) -> bool {
        const std::string key{field};
        if (!obj.contains(key) || obj[key].is_null() || !obj[key].is_number_unsigned()) [[unlikely]] {
                error = std::format("JSON Error: field '{}' is missing or not an unsigned number", field);
                return false;
        }
        out = obj[key].get<std::size_t>();
        return true;
}

[[nodiscard]] static auto verify_digest(std::string_view data, const std::string& expected_digest, std::string& error) -> bool {
        const auto colon{expected_digest.find(':')};
        if (colon == std::string::npos) [[unlikely]] return false;
        const std::string algo{expected_digest.substr(0, colon)};
        std::string actual_digest{};

        if (algo == "sha256") {
                const std::string hash{Utils::sha256(data)};
                if (hash.empty()) return false;
                actual_digest = "sha256:" + hash;
        } else if (algo == "blake3") {
                actual_digest = "blake3:" + blake3_hex(data);
        } else return false;

        if (actual_digest != expected_digest) [[unlikely]] {
                error = std::format("Digest mismatch: expected {}, got {}", expected_digest, actual_digest);
                return false;
        }
        return true;
}

namespace {
struct LayerProgressState {
        std::atomic<std::int64_t> downloaded{0};
        std::int64_t total{0}; // known up front from the manifest, never mutated concurrently
        std::atomic<bool> done{false};
        std::atomic<bool> failed{false};
};

std::mutex g_progress_mutex;
std::unordered_map<std::string, std::shared_ptr<LayerProgressState>> g_progress;
}

[[nodiscard]] static auto short_digest(const std::string& digest) -> std::string {
        const auto colon{digest.find(':')};
        const std::string hex{colon == std::string::npos ? digest : digest.substr(colon + 1)};
        return hex.substr(0, 12);
}

[[nodiscard]] static auto format_mb(std::int64_t bytes) -> std::string {
        return std::format("{:.1f}MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

static auto render_progress(const std::vector<std::string>& order, std::size_t previous_lines) -> std::size_t {
        struct Snapshot { std::string digest; std::int64_t downloaded; std::int64_t total; bool done; bool failed; };
        std::vector<Snapshot> snapshot;
        {
                std::lock_guard<std::mutex> lock(g_progress_mutex);
                snapshot.reserve(order.size());
                for (const auto& d : order) {
                        auto it{g_progress.find(d)};
                        if (it == g_progress.end()) continue;
                        const auto& st{*it->second};
                        snapshot.push_back({d, st.downloaded.load(), st.total, st.done.load(), st.failed.load()});
                }
        }

        if (previous_lines > 0) {
                std::cout << std::format("\033[{}A", previous_lines);
        }

        for (const auto& s : snapshot) {
                std::string status{};
                if (s.failed) {
                        status = "Error";
                } else if (s.done) {
                        status = std::format("Complete   {}", format_mb(s.total));
                } else if (s.total > 0) {
                        const double pct{100.0 * static_cast<double>(s.downloaded) / static_cast<double>(s.total)};
                        status = std::format("Downloading [{:5.1f}%]  {} / {}", pct, format_mb(s.downloaded), format_mb(s.total));
                } else {
                        status = std::format("Downloading  {}", format_mb(s.downloaded));
                }
                std::cout << "\033[K" << short_digest(s.digest) << ": " << status << "\n";
        }
        std::cout.flush();
        return snapshot.size();
}

[[nodiscard]] static auto unpack_with_umoci(const fs::path& layout_dir, const std::string& tag,
                const fs::path& dest_dir, std::string& error) -> bool {
        std::string image_ref{std::format("{}:{}", layout_dir.string(), tag)};

        std::vector<std::string> umoci_args{
                "umoci", "unpack", "--rootless", "--image", image_ref, dest_dir.string()
        };
        std::vector<char*> c_args;
        c_args.reserve(umoci_args.size() + 1);
        for (auto& arg : umoci_args) c_args.push_back(arg.data());
        c_args.push_back(nullptr);

        int err_pipe[2];
        if (pipe(err_pipe) != 0) {
                error = "Failed to create pipe for umoci stderr";
                return false;
        }

        pid_t pid = fork();
        if (pid < 0) {
                error = "Failed to fork process for umoci";
                close(err_pipe[0]);
                close(err_pipe[1]);
                return false;
        }
        else if (pid == 0) {
                close(err_pipe[0]);
                dup2(err_pipe[1], STDERR_FILENO);
                close(err_pipe[1]);
                execvp(c_args[0], c_args.data());
                std::cerr << std::format("Error: failed to execute '{}'\n", c_args[0]);
                _exit(EXIT_FAILURE);
        }

        close(err_pipe[1]);
        std::string umoci_stderr{};
        std::array<char, 4096> buf{};
        ssize_t n{};
        while ((n = read(err_pipe[0], buf.data(), buf.size())) > 0) {
                umoci_stderr.append(buf.data(), static_cast<std::size_t>(n));
        }
        close(err_pipe[0]);

        int status{};
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                return true;
        }

        if (WIFEXITED(status)) {
                error = std::format("umoci unpack failed (exit {}): {}", WEXITSTATUS(status), umoci_stderr);
        } else if (WIFSIGNALED(status)) {
                error = std::format("umoci unpack killed by signal {}: {}", WTERMSIG(status), umoci_stderr);
        } else {
                error = std::format("umoci unpack failed: {}", umoci_stderr);
        }
        return false;
}

auto ImageManager::init() -> void {
        m_images_root = Utils::get_base_dir();
        Utils::ensure_dir(m_images_root);
        const char* home_dir{std::getenv("HOME")};
        if (home_dir == nullptr) {
                struct passwd* pw = getpwuid(getuid());
                if (pw) home_dir = pw->pw_dir;
        }
        if (home_dir) {
                fs::path raw_images = fs::path(home_dir) / ".quiver" / "raw_images";
                Utils::ensure_dir(raw_images);
        }
}

auto ImageManager::pull(const std::string& image_name, std::string& out_path, std::string& error) -> json {
        std::string repo, tag;
        extract_image_meta(image_name, repo, tag);

        if (!is_valid_repo(repo) || !is_valid_tag(tag)) [[unlikely]] {
                error = "Invalid repository or tag name";
                return {};
        }

        std::string token{};
        if (!get_auth_token(repo, "pull", token, error)) [[unlikely]] return {};

        json manifest{};
        std::string media_type{};
        if (!fetch_manifest(repo, tag, token, manifest, media_type, error)) [[unlikely]] {
                secure_zero(token);
                return {};
        }

        // --- FIX: Resolve Manifest List / Index ---
        if (media_type.find("list") != std::string::npos || media_type.find("index") != std::string::npos) {
                std::string target_digest{};
                for (const auto& m : manifest["manifests"]) {
                        // Find linux/amd64 entry
                        if (m.contains("platform") && m["platform"]["architecture"] == "amd64" && m["platform"]["os"] == "linux") {
                                target_digest = m["digest"].get<std::string>();
                                break;
                        }
                }
                if (target_digest.empty()) {
                        error = "No compatible linux/amd64 manifest found in registry index";
                        secure_zero(token);
                        return {};
                }
                // Fetch the specific manifest for this digest
                if (!fetch_manifest(repo, target_digest, token, manifest, media_type, error)) {
                        secure_zero(token);
                        return {};
                }
        }

        if (!manifest.contains("config") || !manifest.contains("layers")) [[unlikely]] {
                error = "Registry Error: Manifest missing required 'config' or 'layers' fields";
                secure_zero(token);
                return {};
        }

        std::string config_digest{};
        if (!json_get_string(manifest["config"], "digest", config_digest, error)) {
                secure_zero(token);
                return {};
        }

        json image_config{};
        if (!fetch_config_blob(repo, config_digest, token, image_config, error)) {
                secure_zero(token);
                return {};
        }

        const char* home_dir{std::getenv("HOME")};
        if (home_dir == nullptr) {
                struct passwd* pw = getpwuid(getuid());
                if (pw) home_dir = pw->pw_dir;
        }
        std::string safe_repo = repo;
        std::replace(safe_repo.begin(), safe_repo.end(), '/', '_');
        std::string safe_tag = tag;
        std::replace(safe_tag.begin(), safe_tag.end(), ':', '_');
        fs::path temp_layout_dir = fs::path(home_dir) / ".quiver" / "raw_images" / std::format("{}_{}", safe_repo, safe_tag);

        std::error_code ec{};
        fs::create_directories(temp_layout_dir / "blobs" / "sha256", ec);
        if (ec) {
                error = std::format("Failed to create layout dir: {}", ec.message());
                secure_zero(token);
                return {};
        }

        fs::path blobs_root{temp_layout_dir / "blobs"};

        std::vector<std::string> layer_order;
        std::vector<std::size_t> layer_sizes;

        struct ProgressCleanup {
                const std::vector<std::string>& order;
                ~ProgressCleanup() {
                        std::lock_guard<std::mutex> lock(g_progress_mutex);
                        for (const auto& d : order) g_progress.erase(d);
                }
        } progress_cleanup{layer_order};

        {
                std::lock_guard<std::mutex> lock(g_progress_mutex);
                for (const auto& layer : manifest["layers"]) {
                        std::string d{}; std::size_t sz{};
                        if (!json_get_string(layer, "digest", d, error) || !json_get_size(layer, "size", sz, error)) {
                                secure_zero(token);
                                return {};
                        }
                        auto state{std::make_shared<LayerProgressState>()};
                        state->total = static_cast<std::int64_t>(sz);
                        g_progress[d] = state;
                        layer_order.push_back(d);
                        layer_sizes.push_back(sz);
                }
        }

        std::vector<std::future<std::string>> download_futures;
        for (std::size_t i{0}; i < layer_order.size(); ++i) {
                download_futures.push_back(std::async(std::launch::async, &ImageManager::download_layer,
                                        this, repo, layer_order[i], token, blobs_root, layer_sizes[i]));
        }

        std::size_t printed_lines{0};
        bool all_ready{false};
        while (!all_ready) {
                printed_lines = render_progress(layer_order, printed_lines);
                all_ready = std::all_of(download_futures.begin(), download_futures.end(), [](auto& f) {
                        return f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
                });
                if (!all_ready) std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
        render_progress(layer_order, printed_lines);

        std::vector<fs::path> layer_blob_paths;
        for (auto& fut : download_futures) {
                std::string path{fut.get()};
                if (path.empty()) {
                        error = "Layer download failed";
                        secure_zero(token);
                        return {};
                }
                layer_blob_paths.emplace_back(std::move(path));
        }

        secure_zero(token);

        const std::string config_bytes{image_config.dump(4)};
        const std::string config_hash{Utils::sha256(config_bytes)};
        if (config_hash.empty()) {
                error = "Failed to hash image config while staging OCI layout";
                return {};
        }
        {
                auto config_path = blobs_root / "sha256" / config_hash;
                if (fs::exists(config_path)) {
                        std::error_code ec;
                        fs::permissions(config_path, fs::perms::owner_write, fs::perm_options::add, ec);
                        fs::remove(config_path, ec);
                }
                std::ofstream cfg_out(config_path, std::ios::binary);
                if (!cfg_out.is_open()) {
                        error = "Failed to write config blob to temp layout";
                        return {};
                }
                cfg_out << config_bytes;
        }
        manifest["config"]["digest"] = "sha256:" + config_hash;
        manifest["config"]["size"]   = config_bytes.size();
        manifest["mediaType"] = "application/vnd.oci.image.manifest.v1+json";
        manifest["config"]["mediaType"] = "application/vnd.oci.image.config.v1+json";
        if (manifest.contains("layers") && manifest["layers"].is_array()) {
                for (auto& l : manifest["layers"]) {
                        if (l.value("mediaType", "") == "application/vnd.docker.image.rootfs.diff.tar.gzip") {
                                l["mediaType"] = "application/vnd.oci.image.layer.v1.tar+gzip";
                        }
                }
        }

        const std::string manifest_bytes{manifest.dump(4)};
        const std::string manifest_hash{Utils::sha256(manifest_bytes)};
        if (manifest_hash.empty()) {
                error = "Failed to hash manifest while staging OCI layout";
                return {};
        }
        {
                auto man_path = blobs_root / "sha256" / manifest_hash;
                if (fs::exists(man_path)) {
                        std::error_code ec;
                        fs::permissions(man_path, fs::perms::owner_write, fs::perm_options::add, ec);
                        fs::remove(man_path, ec);
                }
                std::ofstream man_out(man_path, std::ios::binary);
                if (!man_out.is_open()) {
                        error = "Failed to write manifest blob to temp layout";
                        return {};
                }
                man_out << manifest_bytes;
        }

        const std::string manifest_media_type{
                manifest.value("mediaType", std::string{"application/vnd.oci.image.manifest.v1+json"})
        };

        {
                std::ofstream layout_out(temp_layout_dir / "oci-layout");
                if (!layout_out.is_open()) {
                        error = "Failed to write oci-layout marker";
                        return {};
                }
                layout_out << json{{"imageLayoutVersion", "1.0.0"}}.dump(4);
        }

        {
                json index_json{
                        {"schemaVersion", 2},
                        {"manifests", json::array({
                                json{
                                        {"mediaType", manifest_media_type},
                                        {"digest", "sha256:" + manifest_hash},
                                        {"size", manifest_bytes.size()},
                                        {"annotations", json{{"org.opencontainers.image.ref.name", tag}}}
                                }
                        })}
                };
                std::ofstream index_out(temp_layout_dir / "index.json");
                if (!index_out.is_open()) {
                        error = "Failed to write index.json";
                        return {};
                }
                index_out << index_json.dump(4);
        }

        fs::path img_dest{Utils::get_image_path(image_name)};
        if (fs::exists(img_dest)) fs::remove_all(img_dest, ec);
        Utils::ensure_dir(img_dest.parent_path());

        if (!unpack_with_umoci(temp_layout_dir, tag, img_dest, error)) {
                fs::remove_all(img_dest, ec);
                return {};
        }

        fs::path image_config_path{img_dest / "image_config.json"};
        std::ofstream config_file(image_config_path);
        if (config_file.is_open()) {
                config_file << config_bytes; // already dump(4)-formatted above
                config_file.close();
        } else {
                error = "Failed to write image_config.json to disk";
                return {};
        }

        out_path = img_dest.string();
        return manifest;
}

auto ImageManager::get_auth_token(const std::string& repo, const std::string& scope, std::string& out_token, std::string& error) -> bool {
        auto url{std::format("https://auth.docker.io/token?service=registry.docker.io&scope=repository:{}:{}", repo, scope)};

        cpr::Session session;
        session.SetUrl(cpr::Url{url});
        session.SetTimeout(cpr::Timeout{TIMEOUT_AUTH_MS});

        const char* q_user = std::getenv("QUIVER_USERNAME");
        const char* q_pass = std::getenv("QUIVER_PASSWORD");
        if (q_user != nullptr && q_pass != nullptr) {
                session.SetAuth(cpr::Authentication{q_user, q_pass, cpr::AuthMode::BASIC});
        }

        cpr::Response r = session.Get();

        if (r.status_code != 200) [[unlikely]] {
                error = std::format("Auth Error: HTTP {} - {} - Registry responded: {}", r.status_code, r.error.message, r.text);
                return false;
        }

        json j;
        try {
                j = json::parse(r.text);
        } catch (const json::parse_error& e) {
                error = std::format("Auth Error: JSON Parse Failed - {}", e.what());
                return false;
        }

        // --- FIX: Check for both 'token' and 'access_token' ---
        if (j.contains("token") && j["token"].is_string()) {
                out_token = j["token"].get<std::string>();
        }
        else if (j.contains("access_token") && j["access_token"].is_string()) {
                out_token = j["access_token"].get<std::string>();
        }
        else [[unlikely]] {
                error = std::format("Auth Error: No valid token found in response. Raw body: {}", r.text);
                return false;
        }

        if (out_token.empty()) [[unlikely]] {
                error = "Auth Error: Registry returned an empty token string";
                return false;
        }

        return true;
}
auto ImageManager::fetch_manifest(const std::string& repo, const std::string& tag,
                const std::string& token, json& out_manifest,
                std::string& out_media_type, std::string& error) -> bool {
        auto url{std::format("https://registry-1.docker.io/v2/{}/manifests/{}", repo, tag)};
        cpr::Response r{cpr::Get(
                        cpr::Url{url},
                        cpr::Header{
                        {"Authorization", "Bearer " + token},
                        {"Accept", "application/vnd.docker.distribution.manifest.v2+json, "
                        "application/vnd.docker.distribution.manifest.list.v2+json, "
                        "application/vnd.oci.image.index.v1+json"}
                        },
                        cpr::Timeout{TIMEOUT_MANIFEST_MS}
                        )};

        if (r.status_code != 200) {
                error = std::format("Manifest Error: HTTP {} - {}", r.status_code, r.text);
                return false;
        }
        out_media_type = r.header["Content-Type"];
        try {
                out_manifest = json::parse(r.text);
        } catch (const json::parse_error& e) {
                error = std::format("Manifest Error: JSON Parse Failed - {}", e.what());
                return false;
        }
        return out_manifest.is_object();
}

auto ImageManager::fetch_config_blob(const std::string& repo, const std::string& digest, const std::string& token, json& out_config, std::string& error) -> bool {
        auto url{std::format("https://registry-1.docker.io/v2/{}/blobs/{}", repo, digest)};
        cpr::Response r{cpr::Get(
                        cpr::Url{url},
                        cpr::Header{{"Authorization", "Bearer " + token}, {"Accept", "application/vnd.docker.container.image.v1+json"}},
                        cpr::Redirect{true}, cpr::Timeout{TIMEOUT_CONFIG_MS}
                        )};

        if (r.status_code != 200 || !verify_digest(r.text, digest, error)) return false;
        try {
                out_config = json::parse(r.text);
        } catch (const json::parse_error& e) {
                error = std::format("Config Blob Error: JSON Parse Failed - {}", e.what());
                return false;
        }
        return out_config.is_object();
}

auto ImageManager::download_layer(const std::string& repo, const std::string& digest,
                const std::string& token, const fs::path& dest,
                std::size_t expected_size) -> std::string {
        // Look up this layer's progress-tracking slot, if pull() registered one.
        std::shared_ptr<LayerProgressState> progress{};
        {
                std::lock_guard<std::mutex> lock(g_progress_mutex);
                if (auto it{g_progress.find(digest)}; it != g_progress.end()) progress = it->second;
        }

        // Marks the tracked progress entry done/failed on every return path,
        // success or not, without repeating the bookkeeping at each early return.
        bool succeeded{false};
        struct ProgressFinisher {
                std::shared_ptr<LayerProgressState> p;
                bool* ok;
                ~ProgressFinisher() { if (p) { p->failed.store(!*ok); p->done.store(true); } }
        } finisher{progress, &succeeded};

        if (!is_valid_digest(digest)) [[unlikely]] { std::cerr << "Invalid digest: " << digest << "\n"; return ""; }

        const auto colon{digest.find(':')};
        const std::string algo{digest.substr(0, colon)};
        const std::string hex{digest.substr(colon + 1)};

        auto url{std::format("https://registry-1.docker.io/v2/{}/blobs/{}", repo, digest)};

        // `dest` is expected to be a layout's "blobs" root; land the blob at
        // blobs/<algo>/<hex> per the OCI Image Layout spec, no extension.
        std::error_code dir_ec{};
        fs::create_directories(dest / algo, dir_ec);
        fs::path file_path{dest / algo / hex};

        const fs::path canonical_dest{fs::weakly_canonical(dest)};
        const fs::path canonical_file{fs::weakly_canonical(file_path)};
        if (!canonical_file.string().starts_with(canonical_dest.string())) [[unlikely]] { std::cerr << "Path traversal blocked\n"; return ""; }

        if (fs::exists(file_path)) {
                std::error_code ec;
                if (fs::file_size(file_path, ec) == expected_size) {
                        if (Utils::sha256_file(file_path) == digest) {
                                succeeded = true;
                                if (progress) progress->downloaded.store(expected_size);
                                return digest;
                        }
                }
                // Invalid or partial file, make writable and remove it
                fs::permissions(file_path, fs::perms::owner_write, fs::perm_options::add, ec);
                fs::remove(file_path, ec);
        }

        std::ofstream ofs(file_path, std::ios::binary);
        if (!ofs.is_open()) [[unlikely]] { std::cerr << "Cannot open " << file_path << "\n"; return ""; }

        cpr::Response r{cpr::Get(
                        cpr::Url{url},
                        cpr::Header{
                        {"Authorization", "Bearer " + token},
                        {"Accept-Encoding", "identity"} // <--- ADD THIS to prevent auto-decompression
                        },
                        cpr::Redirect{true},
                        cpr::Timeout{TIMEOUT_LAYER_MS},
                        cpr::WriteCallback([&ofs](std::string_view data, intptr_t) -> bool {
                                ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
                                return ofs.good();
                                }),
                        cpr::ProgressCallback([progress](cpr::cpr_off_t, cpr::cpr_off_t downloadNow,
                                        cpr::cpr_off_t, cpr::cpr_off_t, intptr_t) -> bool {
                                if (progress) progress->downloaded.store(downloadNow);
                                return true; // false would abort the transfer
                                })
                        )};

        // FIX: Must close the file to flush buffers to disk before checking size
        ofs.close();

        if (r.status_code != 200) [[unlikely]] {
                std::cerr << std::format("Download Error: HTTP {} for layer {}\n", r.status_code, digest);
                fs::remove(file_path);
                return "";
        }

        const auto actual_size{fs::file_size(file_path)};
        if (actual_size != expected_size) [[unlikely]] {
                std::cerr << std::format("Size Mismatch: {} (expected {} bytes, got {} bytes)\n",
                                digest, expected_size, actual_size);
                fs::remove(file_path);
                return "";
        }

        const std::string actual_digest{Utils::sha256_file(file_path)};
        if (actual_digest.empty() || actual_digest != digest) [[unlikely]] {
                std::cerr << std::format("Digest Mismatch: expected {} got {}\n", digest, actual_digest);
                fs::remove(file_path);
                return "";
        }

        succeeded = true;
        return file_path.string();
}

auto ImageManager::extract_layer(const fs::path& tarball_path, const fs::path& destination, std::string& error) -> bool {
        try {
                Utils::extract_oci_layer(tarball_path.string(), destination.string());
                return true;
        } catch (const std::exception& e) {
                error = e.what(); return false;
        }
}

auto ImageManager::remove(const std::string& image_name, [[maybe_unused]] std::string& error) -> bool {
        try {
                Utils::remove_directory(Utils::get_image_path(image_name));
        }
        catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
                return false;
        }
        return true;
}

auto ImageManager::extract_image_meta(const std::string& image_name, std::string& repo, std::string& tag) -> void {
        auto pos{image_name.find(':')};
        repo = (pos != std::string::npos) ? image_name.substr(0, pos) : image_name;
        tag  = (pos != std::string::npos) ? image_name.substr(pos + 1) : "latest";
        if (repo.find('/') == std::string::npos) repo = "library/" + repo;
}

auto ImageManager::upload_blob(const std::string& repo, const std::string& digest,
                const std::string& token, const fs::path& filepath,
                std::string& error) -> bool {
        
        auto init_url{std::format("https://registry-1.docker.io/v2/{}/blobs/uploads/", repo)};
        cpr::Response r_init{cpr::Post(
                cpr::Url{init_url},
                cpr::Header{{"Authorization", "Bearer " + token}},
                cpr::Timeout{TIMEOUT_CONFIG_MS}
        )};
        
        if (r_init.status_code != 202) {
                error = std::format("Upload Error: Failed to initiate blob upload. HTTP {} - {}", r_init.status_code, r_init.text);
                return false;
        }
        
        std::string location = r_init.header["Location"];
        if (location.empty()) {
                error = "Upload Error: Registry did not return a Location header";
                return false;
        }
        
        if (location.starts_with("/")) {
                location = "https://registry-1.docker.io" + location;
        }
        
        auto upload_url = std::format("{}&digest={}", location, digest);
        if (location.find('?') == std::string::npos) {
                upload_url = std::format("{}?digest={}", location, digest);
        }
        
        std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
        if (!ifs.is_open()) {
                error = "Upload Error: Cannot open blob file " + filepath.string();
                return false;
        }
        std::streamsize size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        
        std::string buffer(size, 0);
        if (!ifs.read(buffer.data(), size)) {
                error = "Upload Error: Failed to read blob file " + filepath.string();
                return false;
        }
        
        cpr::Response r_upload{cpr::Put(
                cpr::Url{upload_url},
                cpr::Header{
                        {"Authorization", "Bearer " + token},
                        {"Content-Type", "application/octet-stream"}
                },
                cpr::Body{buffer},
                cpr::Timeout{TIMEOUT_LAYER_MS}
        )};
        
        if (r_upload.status_code != 201) {
                error = std::format("Upload Error: Failed to upload blob. HTTP {} - {}", r_upload.status_code, r_upload.text);
                return false;
        }
        
        return true;
}

auto ImageManager::upload_manifest(const std::string& repo, const std::string& tag,
                const std::string& token, const std::string& manifest_json,
                const std::string& media_type, std::string& error) -> bool {
        
        auto url{std::format("https://registry-1.docker.io/v2/{}/manifests/{}", repo, tag)};
        cpr::Response r{cpr::Put(
                cpr::Url{url},
                cpr::Header{
                        {"Authorization", "Bearer " + token},
                        {"Content-Type", media_type}
                },
                cpr::Body{manifest_json},
                cpr::Timeout{TIMEOUT_MANIFEST_MS}
        )};
        
        if (r.status_code != 201 && r.status_code != 200) {
                error = std::format("Upload Error: Failed to push manifest. HTTP {} - {}", r.status_code, r.text);
                return false;
        }
        
        return true;
}

auto ImageManager::push(const std::string& image_name, std::string& error) -> bool {
        std::string repo, tag;
        extract_image_meta(image_name, repo, tag);

        if (!is_valid_repo(repo) || !is_valid_tag(tag)) [[unlikely]] {
                error = "Invalid repository or tag name";
                return false;
        }

        const char* home_dir{std::getenv("HOME")};
        if (home_dir == nullptr) {
                struct passwd* pw = getpwuid(getuid());
                if (pw) home_dir = pw->pw_dir;
        }
        std::string safe_repo = repo;
        std::replace(safe_repo.begin(), safe_repo.end(), '/', '_');
        std::string safe_tag = tag;
        std::replace(safe_tag.begin(), safe_tag.end(), ':', '_');
        fs::path layout_dir = fs::path(home_dir) / ".quiver" / "raw_images" / std::format("{}_{}", safe_repo, safe_tag);

        if (!fs::exists(layout_dir / "index.json")) {
                error = std::format("Push Error: OCI layout for {} does not exist. Did you pull or build it first?", image_name);
                return false;
        }

        std::string token{};
        if (!get_auth_token(repo, "pull,push", token, error)) [[unlikely]] return false;

        std::ifstream index_fs(layout_dir / "index.json");
        json index_json;
        try {
                index_json = json::parse(index_fs);
        } catch (const std::exception& e) {
                error = "Push Error: Failed to parse index.json";
                secure_zero(token);
                return false;
        }

        std::string manifest_digest;
        std::string manifest_media_type;
        for (const auto& m : index_json["manifests"]) {
                manifest_digest = m["digest"];
                manifest_media_type = m["mediaType"];
                break;
        }

        if (manifest_digest.empty()) {
                error = "Push Error: No manifest found in index.json";
                secure_zero(token);
                return false;
        }

        const auto colon = manifest_digest.find(':');
        const std::string m_algo = manifest_digest.substr(0, colon);
        const std::string m_hash = manifest_digest.substr(colon + 1);
        fs::path manifest_path = layout_dir / "blobs" / m_algo / m_hash;

        std::ifstream manifest_fs(manifest_path);
        json manifest_json;
        try {
                manifest_json = json::parse(manifest_fs);
        } catch (const std::exception& e) {
                error = "Push Error: Failed to parse manifest blob";
                secure_zero(token);
                return false;
        }

        std::string config_digest = manifest_json["config"]["digest"];
        const auto c_colon = config_digest.find(':');
        fs::path config_path = layout_dir / "blobs" / config_digest.substr(0, c_colon) / config_digest.substr(c_colon + 1);
        
        std::cout << "Pushing config blob " << config_digest << "\n";
        if (!upload_blob(repo, config_digest, token, config_path, error)) {
                secure_zero(token);
                return false;
        }

        for (const auto& layer : manifest_json["layers"]) {
                std::string l_digest = layer["digest"];
                const auto l_colon = l_digest.find(':');
                fs::path l_path = layout_dir / "blobs" / l_digest.substr(0, l_colon) / l_digest.substr(l_colon + 1);
                
                std::cout << "Pushing layer blob " << l_digest << "\n";
                if (!upload_blob(repo, l_digest, token, l_path, error)) {
                        secure_zero(token);
                        return false;
                }
        }

        std::cout << "Pushing manifest " << tag << "\n";
        std::ifstream manifest_fs_raw(manifest_path);
        std::stringstream buffer;
        buffer << manifest_fs_raw.rdbuf();
        
        if (!upload_manifest(repo, tag, token, buffer.str(), manifest_media_type, error)) {
                secure_zero(token);
                return false;
        }

        secure_zero(token);
        return true;
}

ImageManager::~ImageManager() {}
