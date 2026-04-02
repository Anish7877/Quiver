#include "image_manager.hpp"
#include "utils.hpp"
#include "serialization.hpp"
// #include "database_command_queue.hpp" // Commented out — DB subsystem disabled
// #include "logger_command_queue.hpp"   // Commented out — logger subsystem disabled
// #include "value_heap.hpp"             // Commented out — logger subsystem disabled
#include <cpr/cpr.h>
#include <blake3.h>
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
#include <pwd.h>

namespace chrono = std::chrono;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr std::size_t  MAX_TOKEN_RESPONSE_SIZE {   64 * 1024}; 
static constexpr std::size_t  MAX_MANIFEST_SIZE       {    1 * 1024 * 1024};
static constexpr std::size_t  MAX_CONFIG_SIZE         {    1 * 1024 * 1024};

static constexpr std::int32_t TIMEOUT_AUTH_MS         {    5'000};
static constexpr std::int32_t TIMEOUT_MANIFEST_MS     {    5'000};
static constexpr std::int32_t TIMEOUT_CONFIG_MS       {   10'000};
static constexpr std::int32_t TIMEOUT_LAYER_MS        {  300'000};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

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
    static const std::regex valid{R"(^[a-z0-9_\-]+(\/[a-z0-9_\-]+)*$)"};
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

// ---------------------------------------------------------------------------
// ImageManager Implementation
// ---------------------------------------------------------------------------

auto ImageManager::init() -> void {
    m_images_root = Utils::get_base_dir();
    Utils::ensure_dir(m_images_root);
}

auto ImageManager::pull(const std::string& image_name, std::string& out_path, std::string& error) -> json {
    std::string repo, tag;
    extract_image_meta(image_name, repo, tag);

    if (!is_valid_repo(repo) || !is_valid_tag(tag)) [[unlikely]] {
        error = "Invalid repository or tag name";
        return {};
    }

    std::string token{};
    if (!get_auth_token(repo, token, error)) [[unlikely]] return {};

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

    // Guard manifest structure
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
    //--------------------------------------------------------------------------------------------------------
    std::string safeImageName = image_name;
    std::replace(safeImageName.begin(), safeImageName.end(), ':', '_');
    std::replace(safeImageName.begin(), safeImageName.end(), '/', '_');
    
    // This will correctly resolve to: /home/kartik-goel/.quiver/alpine_latest
    fs::path img_dest{m_images_root / "images" / safeImageName};
    Utils::ensure_dir(img_dest);

    std::vector<std::future<std::string>> download_futures;
    for (const auto& layer : manifest["layers"]) {
        std::string layer_digest{};
        std::size_t layer_size{};
        if (!json_get_string(layer, "digest", layer_digest, error) || !json_get_size(layer, "size", layer_size, error)) {
            secure_zero(token);
            return {};
        }
        download_futures.push_back(std::async(std::launch::async, &ImageManager::download_layer, 
                                   this, repo, layer_digest, token, img_dest, layer_size));
    }

    std::vector<std::string> tar_paths;
    for (auto& fut : download_futures) {
        std::string path{fut.get()};
        if (path.empty()) {
            error = "Layer download failed";
            secure_zero(token);
            return {};
        }
        tar_paths.push_back(std::move(path));
    }

    secure_zero(token);

    for (const auto& tar_path : tar_paths) {
        if (!extract_layer(fs::path(tar_path), img_dest, error)) return {};
        fs::remove(tar_path);
    }

    fs::path config_path = img_dest / "config.json";
    std::ofstream config_file(config_path);
    if (config_file.is_open()) {
        config_file << image_config.dump(4); // dump(4) adds nice indentation
        config_file.close();
    } else {
        error = "Failed to write config.json to disk";
        return {};
    }

    out_path = img_dest.string();
    return image_config;
}

auto ImageManager::get_auth_token(const std::string& repo, std::string& out_token, std::string& error) -> bool {
    auto url{std::format("https://auth.docker.io/token?service=registry.docker.io&scope=repository:{}:pull", repo)};
    
    cpr::Response r{cpr::Get(cpr::Url{url}, cpr::Timeout{TIMEOUT_AUTH_MS})};
    
    if (r.status_code != 200) [[unlikely]] {
        error = std::format("Auth Error: HTTP {} - Registry responded: {}", r.status_code,r.error.message, r.text);
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
        error = std::format("Manifest Error: ({})", r.status_code);
        return false;
    }
    out_media_type = r.header["Content-Type"];
    out_manifest = json::parse(r.text);
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
    out_config = json::parse(r.text);
    return out_config.is_object();
}

auto ImageManager::download_layer(const std::string& repo, const std::string& digest,
                                  const std::string& token, const fs::path& dest,
                                  std::size_t expected_size) -> std::string {
    if (!is_valid_digest(digest)) [[unlikely]] return "";

    auto url{std::format("https://registry-1.docker.io/v2/{}/blobs/{}", repo, digest)};
    fs::path file_path{dest / std::format("{}.tar.gz", digest.substr(7, 12))};

    const fs::path canonical_dest{fs::weakly_canonical(dest)};
    const fs::path canonical_file{fs::weakly_canonical(file_path)};
    if (!canonical_file.string().starts_with(canonical_dest.string())) [[unlikely]] return "";

    std::ofstream ofs(file_path, std::ios::binary);
    if (!ofs.is_open()) [[unlikely]] return "";

    cpr::Response r{cpr::Get(
        cpr::Url{url}, 
        cpr::Header{{"Authorization", "Bearer " + token}}, 
        cpr::Redirect{true},
        cpr::Timeout{TIMEOUT_LAYER_MS},
        cpr::WriteCallback([&ofs](std::string_view data, intptr_t) -> bool {
            ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
            return true;
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

    return file_path.string();
}

auto ImageManager::extract_layer(const fs::path& tarball_path, const fs::path& destination, std::string& error) -> bool {
    try {
        Utils::extract_tarball(tarball_path.string(), destination.string());
        return true;
    } catch (const std::exception& e) {
        error = e.what(); return false;
    }
}

auto ImageManager::remove(const std::string& image_name, [[maybe_unused]] std::string& error) -> bool {
    std::string repo, tag;
    extract_image_meta(image_name, repo, tag);
    return Utils::remove_directory(m_images_root / repo / tag);
}

auto ImageManager::extract_image_meta(const std::string& image_name, std::string& repo, std::string& tag) -> void {
    auto pos{image_name.find(':')};
    repo = (pos != std::string::npos) ? image_name.substr(0, pos) : image_name;
    tag  = (pos != std::string::npos) ? image_name.substr(pos + 1) : "latest";
    if (repo.find('/') == std::string::npos) repo = "library/" + repo;
}

ImageManager::~ImageManager() {}
