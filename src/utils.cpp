#include "log_job_processor.hpp"
#include "oci_runtime.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <array>
#include <atomic>
#include <blake3.h>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <pwd.h>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <zlib.h>

auto Utils::dir_exists(const fs::path& path) -> bool {
        return fs::is_directory(path);
}

auto Utils::file_exists(const fs::path& path) -> bool {
        return fs::is_regular_file(path);
}

auto Utils::ensure_dir(const fs::path& path, mode_t mode) -> void {
        if (!dir_exists(path)) {
                std::error_code error_code{};
                fs::create_directories(path, error_code);

                if (error_code) [[unlikely]] {
                        throw std::runtime_error(std::format("Directory Error: couldn't create '{}' - {}\n", path.string(), error_code.message()));
                }

                fs::permissions(path, static_cast<fs::perms>(mode), fs::perm_options::replace, error_code);
                if (error_code) [[unlikely]] {
                        throw std::runtime_error(std::format("Permissions Error: couldn't set permissions for '{}' - {}\n", path.string(), error_code.message()));
                }
        }
}

auto Utils::ensure_file(const fs::path& path) -> void {
        if (!file_exists(path)) {
                fs::path parent_path{path.parent_path()};
                if(!parent_path.empty() && !dir_exists(parent_path)) ensure_dir(parent_path);
                std::ofstream file{path};
                if(!file) [[unlikely]] {
                        throw std::runtime_error(std::format("File Error: failed to create '{}'\n", path.string()));
                }
        }
}

auto Utils::write_file(const fs::path& path, std::string_view buffer, bool append_mode) -> void {
        fs::path parent_path{path.parent_path()};
        if (!parent_path.empty() && !dir_exists(parent_path)) ensure_dir(parent_path);

        std::ios_base::openmode mode{std::ios::out};
        if (append_mode) {
                mode |= std::ios::app;
        }
        std::ofstream file{path, mode};

        if (!file.is_open()) [[unlikely]] {
                throw std::runtime_error(std::format("File Error: couldn't open '{}'\n", path.string()));
        }
        file << buffer;
        if (!file) [[unlikely]] {
                throw std::runtime_error(std::format("File Error: failed to write data to '{}'\n", path.string()));
        }
}

auto Utils::copy_directory(const fs::path& source, const fs::path& destination) -> void {
        std::error_code error_code{};
        fs::copy(source, destination, fs::copy_options::recursive | fs::copy_options::copy_symlinks, error_code);
        if (error_code) [[unlikely]] {
                throw std::runtime_error(std::format("Directory Error: couldn't copy '{}' -> '{}' with error - {}\n",
                                source.string(), destination.string(), error_code.message()));
        }
}

auto Utils::remove_directory(const fs::path& path) -> void {
        std::error_code error_code{};
        fs::remove_all(path, error_code);
        if (error_code) [[unlikely]] {
                throw std::runtime_error(std::format("Directory Error: couldn't remove '{}' - {}\n", path.string(), error_code.message()));
        }
}

auto Utils::rename_file_or_directory(const fs::path& src, const fs::path& dst) -> void {
        std::error_code error_code{};
        fs::rename(src, dst, error_code);
        if (error_code) [[unlikely]] {
                throw std::runtime_error(std::format("Directory Error: couldn't rename '{}' to '{}' - {}\n", src.string(),
                                        dst.string(), error_code.message()));
        }
}

auto Utils::change_permissions(const fs::path& path, mode_t mode) -> void {
        std::error_code error_code{};
        fs::permissions(path, static_cast<fs::perms>(mode), fs::perm_options::replace, error_code);
        if (error_code) [[unlikely]] {
                throw std::runtime_error(std::format("Directory Error: couldn't change permissions for '{}' - '{}'\n",
                                        path.string(),error_code.message()));
        }
        if (fs::is_directory(path)) {
                for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                        if (entry.is_symlink()) continue;
                        fs::permissions(entry.path(), static_cast<fs::perms>(mode), fs::perm_options::replace, error_code);
                        if (error_code) [[unlikely]] {
                                throw std::runtime_error(std::format("Directory Error: couldn't change permissions for '{}' - '{}'\n",
                                                        entry.path().string(),error_code.message()));
                        }
                }
        }
}

auto Utils::change_owners(const fs::path& path, uid_t uid, gid_t gid) -> void {
        if (chown(path.c_str(), uid, gid) == -1) [[unlikely]] {
                throw std::runtime_error(std::format("Directory Error: couldn't change owner for '{}' - '{}'\n",
                                        path.string(), std::strerror(errno)));
        }
        if (fs::is_directory(path)) {
                for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                        if (chown(entry.path().c_str(), uid, gid) == -1) [[unlikely]] {
                                throw std::runtime_error(std::format("Directory Error: couldn't change owner for '{}' - '{}'\n",
                                                        entry.path().string(), std::strerror(errno)));
                        }
                }
        }
}

auto Utils::get_base_dir() -> fs::path {
        const char* home{getenv("HOME")};
        std::string base{home ? std::string(home) : "/tmp"};
        return base + "/.quiver";
}

auto Utils::get_sock_path(std::string_view container_id) -> fs::path {
        std::string path{std::format("/tmp/quiver_{}.sock", container_id)};
        return path;
}

auto Utils::get_filesystem_path(std::string_view container_id) -> fs::path {
        std::string path{std::format("{}/filesystems/{}",get_base_dir().string(), container_id)};
        return path;
}

auto Utils::get_vfs_path(std::string_view container_id) -> fs::path {
        std::string path{std::format("{}/vfs/{}",get_base_dir().string(), container_id)};
        return path;
}

auto Utils::get_layers_path(std::string_view layer_name) -> fs::path {
        std::string path{std::format("{}/layers/{}", get_base_dir().string(), layer_name)};
        return path;
}

auto Utils::get_image_path(std::string_view image_name) -> fs::path {
        std::string path{std::format("{}/images/{}", get_base_dir().string(), image_name)};
        return path;
}

auto Utils::get_db_path(std::string_view name) -> fs::path {
        std::string path{std::format("{}/db/{}", get_base_dir().string(), name)};
        return path;
}

auto Utils::get_log_path(std::string_view name) -> fs::path {
        std::string path{std::format("{}/logs/{}.log", get_base_dir().string(), name)};
        return path;
}

auto Utils::get_logger_command_queue_buf_name() -> std::string {
        return "log_command_queue";
}

auto Utils::get_database_command_queue_buf_name() -> std::string {
        return "db_command_queue";
}

auto Utils::get_value_heap_buf_name() -> std::string {
        return "value_heap";
}

auto Utils::get_device_gid(const fs::path& device) -> gid_t {
        struct stat file_info{};

        if(stat(device.c_str(), &file_info) == -1) [[unlikely]] {
                std::cerr << "Device Error: could not read device file.\n";
                return -1;
        }
        return file_info.st_gid;
}

auto Utils::get_gid_map_payload(const std::vector<OCIRuntime::Device>& devices) -> std::string {
        std::string payload{};
        std::set<gid_t> unique_gids{};

        for (const auto& device : devices) {
                gid_t gid{get_device_gid(device.host_path)};

                if (unique_gids.insert(gid).second) {
                        payload += std::format("{} {} 1\n", gid, gid);
                }
        }
        return payload;
}

auto Utils::find_program_path(const std::string& program_name) -> fs::path {
        if (program_name.empty()) return "";
        if (program_name[0] == '/') {
                if (fs::exists(program_name) && fs::is_regular_file(program_name)) {
                        if (access(program_name.c_str(), X_OK) != -1) {
                                return program_name;
                        }
                }
                return "";
        }

        const char* path_env{std::getenv("PATH")};
        if (!path_env) return "";

        std::stringstream ss{path_env};
        std::string dir{};

        while (std::getline(ss, dir, ':')) {
                fs::path candidate{fs::path(dir) / program_name};

                if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
                        if (access(candidate.c_str(), X_OK) != -1) {
                                return candidate;
                        }
                }
        }
        return "";
}

auto Utils::generate_container_id() -> std::string {
        auto now{std::chrono::system_clock::now().time_since_epoch().count()};
        std::random_device rd{};
        std::string input{std::to_string(now) + ":" + std::to_string(rd())};

        blake3_hasher hasher{};
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, input.c_str(), input.length());

        uint8_t output[BLAKE3_OUT_LEN];
        blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
        std::stringstream hex_stream{};
        hex_stream << std::hex << std::setfill('0');
        for (std::size_t i{0}; i < BLAKE3_OUT_LEN; ++i) {
                hex_stream << std::setw(2) << static_cast<int>(output[i]);
        }
        return hex_stream.str();
}

auto Utils::spawn_new_consumer() -> pid_t {
        int sync_pipe[2];
        if (pipe(sync_pipe) == -1) {
                return -1;
        }
        pid_t intermediate_pid{fork()};

        if (intermediate_pid == -1) {
                std::ofstream job_processor_log{get_log_path("log_processor"), std::ios::app};
                job_processor_log << std::format("Unable to spawn new job processor: {}\n", std::strerror(errno));
                close(sync_pipe[0]);
                close(sync_pipe[1]);
                return -1;
        }

        if (intermediate_pid > 0) {
                close(sync_pipe[1]);
                pid_t consumer_pid{0};
                ssize_t n = read(sync_pipe[0], &consumer_pid, sizeof(consumer_pid));
                close(sync_pipe[0]);
                waitpid(intermediate_pid, nullptr, 0);
                if (n != static_cast<ssize_t>(sizeof(consumer_pid)) || consumer_pid <= 0) {
                        return -1;
                }
                return consumer_pid;
        }

        close(sync_pipe[0]);
        pid_t consumer_pid{fork()};
        if (consumer_pid > 0) {
                close(sync_pipe[1]);
                _exit(EXIT_SUCCESS);
        }
        if (consumer_pid == -1) {
                close(sync_pipe[1]);
                _exit(EXIT_FAILURE);
        }

        if (setsid() == -1) {
                close(sync_pipe[1]);
                _exit(EXIT_FAILURE);
        }

        int null_fd{open("/dev/null", O_RDWR)};
        if (null_fd != -1) {
                dup2(null_fd, STDIN_FILENO);
                dup2(null_fd, STDOUT_FILENO);
                dup2(null_fd, STDERR_FILENO);
                close(null_fd);
        }

        int fd{open("/tmp/quiver_job_processor.lock", O_CREAT | O_RDWR, 0644)};
        if (fd == -1) {
                close(sync_pipe[1]);
                _exit(EXIT_FAILURE);
        }

        if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
                close(fd);
                close(sync_pipe[1]);
                _exit(EXIT_SUCCESS);
        }

        auto handler{[](int signum) -> void {
                        job_processor_running.store(false, std::memory_order_release);
        }};
        std::signal(SIGTERM, handler);
        std::signal(SIGINT, handler);
        auto& log_job_processor{LogJobProcessor::get_instance()};
        log_job_processor.init();
        pid_t my_pid{getpid()};
        if (write(sync_pipe[1], &my_pid, sizeof(my_pid)) != static_cast<ssize_t>(sizeof(my_pid))) {
                close(sync_pipe[1]);
                _exit(EXIT_FAILURE);
        }
        close(sync_pipe[1]);
        log_job_processor.process_job();

        while (job_processor_running.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        log_job_processor.stop();

        if (flock(fd, LOCK_UN) == -1) {
                close(fd);
                _exit(EXIT_FAILURE);
        }
        close(fd);

        _exit(EXIT_SUCCESS);
}

auto Utils::parse_subgid(const std::string& username) -> std::vector<SubIDRange> {
        std::ifstream file("/etc/subgid");
        std::vector<SubIDRange> ranges{};

        std::string line{};
        while (std::getline(file, line)) {
                std::stringstream ss(line);
                std::string user{};
                uint32_t start{}, count{};

                if (std::getline(ss, user, ':')
                                && ss >> start
                                && ss.ignore(1)
                                && ss >> count) {
                        if (user == username) {
                                ranges.push_back({start, count});
                        }
                }
        }
        return ranges;
}

auto Utils::parse_subuid(const std::string& username) -> std::vector<SubIDRange> {
        std::ifstream file("/etc/subuid");
        std::vector<SubIDRange> ranges{};

        std::string line{};
        while (std::getline(file, line)) {
                std::stringstream ss(line);
                std::string user{};
                uint32_t start{}, count{};

                if (std::getline(ss, user, ':')
                                && ss >> start
                                && ss.ignore(1)
                                && ss >> count) {
                        if (user == username) {
                                ranges.push_back({start, count});
                        }
                }
        }
        return ranges;
}

auto Utils::get_username() -> std::string {
        uid_t uid{getuid()};
        struct passwd* pw{getpwuid(uid)};
        if (!pw) throw std::runtime_error("Failed to get username.");
        return std::string(pw->pw_name);
}

auto Utils::build_gid_map_payload(pid_t pid) -> std::string {
        std::string username{get_username()};
        gid_t host_gid{getgid()};
        auto ranges{parse_subgid(username)};
        if (ranges.empty()) {
                throw std::runtime_error("No subgid range found");
        }
        std::stringstream payload{""};
        payload << "0 " << host_gid << " 1\n";
        uint32_t container_id{1};
        for (const auto& r : ranges) {
                payload << container_id << " " << r.start << " " << r.count << "\n";
                container_id += r.count;
        }
        return payload.str();
}

auto Utils::write_all(int fd, const char* buf, ssize_t len) -> bool {
        ssize_t off{0};
        while (off < len) {
                ssize_t w = write(fd, buf + off, len - off);
                if (w > 0) {
                        off += w;
                        continue;
                }
                if (w < 0 && errno == EINTR)
                        continue;
                return false;
        }
        return true;
}


auto Utils::create_tar_gz(const fs::path& path, const fs::path& output_file) -> void {
        struct archive* archive{archive_write_new()};
        if (!archive) [[unlikely]]
                throw std::runtime_error("Failed to create archive write object");
        std::unique_ptr<struct archive, decltype(&archive_write_free)> archive_guard{archive, archive_write_free};
        if (archive_write_add_filter_gzip(archive) != ARCHIVE_OK) [[unlikely]]
                throw std::runtime_error(archive_error_string(archive));
        if (archive_write_set_format_pax_restricted(archive) != ARCHIVE_OK) [[unlikely]]
                throw std::runtime_error(archive_error_string(archive));
        if (archive_write_open_filename(archive, output_file.c_str()) != ARCHIVE_OK) {
                throw std::runtime_error(archive_error_string(archive));
        }

        std::array<char, 8192> buffer{};
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
                fs::path relative{fs::relative(entry.path(), path)};
                archive_entry* ae{archive_entry_new()};
                archive_entry_set_pathname(ae, relative.generic_string().c_str());
                if (entry.is_directory()) {
                        archive_entry_set_filetype(ae, AE_IFDIR);
                        archive_entry_set_perm(ae, 0755);
                        archive_entry_set_size(ae, 0);
                        if (archive_write_header(archive, ae) != ARCHIVE_OK) [[unlikely]]
                                std::cerr << std::format("Tar Warning: {}\n", archive_error_string(archive));
                }
                else if (entry.is_regular_file()) {
                        archive_entry_set_filetype(ae, AE_IFREG);
                        archive_entry_set_perm(ae, static_cast<mode_t>(fs::status(entry.path()).permissions()) & 07777);
                        archive_entry_set_size(ae, fs::file_size(entry.path()));
                        if (archive_write_header(archive, ae) != ARCHIVE_OK) [[unlikely]]
                                std::cerr << std::format("Tar Warning: {}\n", archive_error_string(archive));
                        std::ifstream in(entry.path(), std::ios::binary);
                        while (in) {
                                in.read(buffer.data(), buffer.size());
                                auto n{in.gcount()};
                                if (n > 0) {
                                        if (archive_write_data(archive, buffer.data(), static_cast<size_t>(n)) == 0) [[unlikely]]
                                                std::cerr << "Tar Warning: Failed to write data\n";
                                }
                        }
                }
                else if (entry.is_symlink()) {
                        archive_entry_set_filetype(ae, AE_IFLNK);
                        auto target{fs::read_symlink(entry.path())};
                        archive_entry_set_symlink(ae, target.generic_string().c_str());
                        archive_entry_set_size(ae, 0);
                        if (archive_write_header(archive, ae) != ARCHIVE_OK) [[unlikely]]
                                std::cerr << std::format("Tar Warning: {}\n", archive_error_string(archive));
                }
                archive_entry_free(ae);
        }
        archive_write_close(archive);
}

auto Utils::extract_tarball(const std::string& tarball_path, const std::string& destination_path) -> void {
        struct archive* a;
        struct archive* ext;
        struct archive_entry* entry;

        // Standard extraction flags: preserve time, permissions, ACLs, flags, and security checks
        int flags{ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS |
                  ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS | ARCHIVE_EXTRACT_SECURE_SYMLINKS};
        int r;

        a = archive_read_new();
        archive_read_support_format_all(a);
        archive_read_support_filter_all(a);

        ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, flags);
        archive_write_disk_set_standard_lookup(ext);

        std::unique_ptr<struct archive, decltype(&archive_read_free)> a_guard{a, archive_read_free};
        std::unique_ptr<struct archive, decltype(&archive_write_free)> ext_guard{ext, archive_write_free};

        if (archive_read_open_filename(a, tarball_path.c_str(), 10240) != ARCHIVE_OK) [[unlikely]] {
                throw std::runtime_error(std::format("Tar Error: Could not open {} - {}", tarball_path, archive_error_string(a)));
        }

        // Pre-calculate the canonical destination to use as a security boundary
        const fs::path canonical_dest{fs::weakly_canonical(destination_path)};

        while (true) {
                r = archive_read_next_header(a, &entry);
                if (r == ARCHIVE_EOF) break;
                if (r < ARCHIVE_OK) [[unlikely]] {
                        std::cerr << std::format("Tar Warning: {}\n", archive_error_string(a));
                        if (r < ARCHIVE_WARN) throw std::runtime_error("Tar Critical Error");
                }

                // --- SECURITY CHECK: Path Traversal Guard ---
                std::string raw_path = archive_entry_pathname(entry);

                // 1. Strip leading slashes to prevent absolute path overriding
                while (!raw_path.empty() && raw_path.front() == '/') {
                        raw_path.erase(0, 1);
                }

                // 2. Lexically normalize (resolves . and .. textually, WITHOUT touching the host disk)
                fs::path entry_path{raw_path};
                entry_path = entry_path.lexically_normal();

                // 3. Block path traversal purely based on the normalized string
                if (entry_path.string().starts_with("..")) [[unlikely]] {
                        std::cerr << std::format("Security Warning: Blocked path traversal attempt in tarball: {}\n", entry_path.string());
                        continue;
                }

                // 4. Safely construct the final absolute path
                const fs::path full_path{canonical_dest / entry_path};
                archive_entry_set_pathname(entry, full_path.c_str());

                // 5. Handle hard links with the exact same string-only normalization
                const char* hardlink_target = archive_entry_hardlink(entry);
                if (hardlink_target != nullptr) {
                        std::string raw_hl = hardlink_target;
                        while (!raw_hl.empty() && raw_hl.front() == '/') {
                                raw_hl.erase(0, 1);
                        }

                        fs::path hl_path{raw_hl};
                        hl_path = hl_path.lexically_normal();

                        if (hl_path.string().starts_with("..")) {
                                std::cerr << std::format("Security Warning: Blocked hardlink traversal attempt in tarball: {}\n", hl_path.string());
                                continue;
                        }
                        fs::path full_hardlink_path = canonical_dest / hl_path;
                        archive_entry_set_hardlink(entry, full_hardlink_path.c_str());
                }
                r = archive_write_header(ext, entry);
                if (r < ARCHIVE_OK) [[unlikely]] {
                        std::cerr << std::format("Tar Error: failed to write header for {} - {}\n",
                                        archive_entry_pathname(entry), archive_error_string(ext));
                } else if (archive_entry_size(entry) > 0) {
                        // Copy the data from the archive to the disk
                        const void* buff;
                        size_t size;
                        la_int64_t offset;

                        while (true) {
                                r = archive_read_data_block(a, &buff, &size, &offset);
                                if (r == ARCHIVE_EOF) break;
                                if (r < ARCHIVE_OK) [[unlikely]] break;

                                r = archive_write_data_block(ext, buff, size, offset);
                                if (r < ARCHIVE_OK) [[unlikely]] {
                                        std::cerr << std::format("Tar Error: data write failed - {}\n", archive_error_string(ext));
                                        break;
                                }
                        }
                }

                r = archive_write_finish_entry(ext);
                if (r < ARCHIVE_OK) [[unlikely]] {
                        std::cerr << std::format("Tar Error: finish entry failed - {}\n", archive_error_string(ext));
                }
        }
}

auto Utils::is_archive(const fs::path& path) -> bool {
        archive* a{archive_read_new()};
        if (a == nullptr) {
                throw std::runtime_error("Tar Error: Failed to create archive reader.");
        }
        archive_read_support_filter_all(a);
        archive_read_support_format_all(a);

        if (archive_read_open_filename(a, path.c_str(), 10240) != ARCHIVE_OK) {
                archive_read_free(a);
                return false;
        }
        archive_entry* entry{};
        bool is_arc{archive_read_next_header(a, &entry) ==  ARCHIVE_OK};
        archive_read_close(a);
        archive_read_free(a);
        return is_arc;
}

struct EvpCtxDeleter {
        void operator()(EVP_MD_CTX* ctx) const noexcept { EVP_MD_CTX_free(ctx); }
};
using EvpCtxPtr = std::unique_ptr<EVP_MD_CTX, EvpCtxDeleter>;


auto Utils::sha256(std::string_view data) -> std::string {
        EvpCtxPtr context{EVP_MD_CTX_new()};
        if (context == nullptr) [[unlikely]] {
                return "";
        }

        if (!EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr)) [[unlikely]] {
                return "";
        }

        if (!EVP_DigestUpdate(context.get(), data.data(), data.size())) [[unlikely]] {
                return "";
        }

        std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
        unsigned int len{};
        if (!EVP_DigestFinal_ex(context.get(), hash.data(), &len)) [[unlikely]] {
                return "";
        }

        std::ostringstream oss{};
        oss << std::hex << std::setfill('0');
        for (unsigned int i{0}; i < len; ++i) {
                oss << std::setw(2) << static_cast<unsigned>(hash[i]);
        }
        return oss.str();
}

auto Utils::sha256_file(const fs::path& file) -> std::string {
        EvpCtxPtr ctx{EVP_MD_CTX_new()};
        if (!ctx)
                throw std::runtime_error("Failed to create EVP context");
        if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1)
                throw std::runtime_error("EVP_DigestInit_ex failed");
        std::ifstream in(file, std::ios::binary);
        std::array<char, 8192> buffer{};
        while (in) {
                in.read(buffer.data(), buffer.size());
                auto n = in.gcount();
                if (n > 0) {
                        if (EVP_DigestUpdate(ctx.get(), buffer.data(), static_cast<size_t>(n)) != 1)
                                throw std::runtime_error("EVP_DigestUpdate failed");
                }
        }
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int digest_len{};
        if (EVP_DigestFinal_ex(ctx.get(), digest.data(), &digest_len) != 1)
                throw std::runtime_error("EVP_DigestFinal_ex failed");
        std::stringstream ss{};
        ss << std::hex << std::setfill('0');
        for (size_t i{0}; i < digest_len; ++i)
                ss << std::setw(2) << static_cast<unsigned>(digest[i]);
        return "sha256:" + ss.str();
}

auto Utils::print_usage() -> void { std::cout << "Usage: quiver <command> [options] [arguments]\n\n" << "Commands:\n"
                << "  run [options] -i <image> [cmd]   Create and start a new container\n"
                << "      -i, --image <name>           Image to use (required)\n"
                << "      -n, --name <name>            Assign a name to the container\n"
                << "      -p, --port <host:cont>       Publish a container's port(s) to the host\n"
                << "      -v, --volume <host:cont>     Bind mount a volume\n"
                << "      --vfs                        Use VFS (copy) instead of OverlayFS for package manager related\n"
                << "      --no-remove                  Do not remove the filesystem after exit\n\n"

                << "  start <container_id> ...         Start one or more stopped containers\n"
                << "  stop <container_id> ...          Stop one or more running containers\n"
                << "  rm <container_id> ...            Remove one or more containers\n"
                << "  attach <container_id>            Attach local standard input, output, and error to a running container\n\n"

                << "  ps [-a]                          List containers\n"
                << "      -a                           Show all containers (default shows just running)\n\n"

                << "  pull <image_name>                Pull an image from a registry\n\n"
                << "  image <subcommand>               Manage images\n"
                << "      ls                           List available images\n"
                << "      rm <image:tag>               Remove an image\n"
                << "      cls <image_name>             List containers using a specific image\n\n"

                << "  volume <subcommand>              Manage volumes\n"
                << "      ls                           List all volumes\n"
                << "      rm <volume_id> ...           remove one or more volume links\n\n"

                << "  network <subcommand>             Manage networks\n"
                << "      ls                           List all network port mappings\n"
                << "      rm <network_id> ...          remove one or more network links\n"
                << "      add <container_id> [args]                                    \n"
                << "              <host:cont> ...      add a new network link\n\n"
                << "  create <subcommand>              Create resources\n"
                << "      volume <container_id> [args]                 \n"
                << "                  <host:cont> ...  Create a volume link for container\n\n"
                << "  vfs rm <container_id>            remove vfs for a container\n\n"
                << "  help                             Show this help message\n";
}

// ---------------------------------------------------------------------------
// OCI Layer Export Utilities
// ---------------------------------------------------------------------------

auto Utils::sha256_final(EVP_MD_CTX* ctx) -> std::string {
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int len{};
        if (EVP_DigestFinal_ex(ctx, digest.data(), &len) != 1) {
                throw std::runtime_error("OCI Layer Error: EVP_DigestFinal_ex failed");
        }
        std::ostringstream oss{};
        oss << std::hex << std::setfill('0');
        for (unsigned int i{0}; i < len; ++i) {
                oss << std::setw(2) << static_cast<unsigned>(digest[i]);
        }
        return oss.str();
}

auto Utils::is_overlay_whiteout(const fs::path& path) -> bool {
        struct stat st{};
        if (lstat(path.c_str(), &st) != 0) {
                return false;
        }
        return S_ISCHR(st.st_mode) && major(st.st_rdev) == 0 && minor(st.st_rdev) == 0;
}

auto Utils::is_opaque_directory(const fs::path& path) -> bool {
        char value[16]{};
        // Try the trusted.overlay.opaque xattr first (privileged overlay)
        ssize_t len = getxattr(path.c_str(), "trusted.overlay.opaque", value, sizeof(value));
        if (len > 0 && value[0] == 'y') {
                return true;
        }
        // Try user.overlay.opaque xattr (rootless/user namespace overlay)
        len = getxattr(path.c_str(), "user.overlay.opaque", value, sizeof(value));
        if (len > 0 && value[0] == 'y') {
                return true;
        }
        return false;
}

// ---------------------------------------------------------------------------
// OCI Layer: custom archive write callback context
// ---------------------------------------------------------------------------

namespace {

struct OciLayerContext {
        EVP_MD_CTX* diff_ctx{nullptr};   // SHA256 of uncompressed tar
        EVP_MD_CTX* blob_ctx{nullptr};   // SHA256 of compressed gzip
        z_stream zstrm{};
        FILE* output_fp{nullptr};
        std::uint64_t compressed_size{0};
        std::array<unsigned char, 65536> zbuf{};

        OciLayerContext(EVP_MD_CTX* d, EVP_MD_CTX* b, FILE* fp)
                : diff_ctx{d}, blob_ctx{b}, output_fp{fp} {}
};

auto oci_layer_write_cb(struct archive* /*ar*/, void* client_data, const void* buffer, size_t length) -> la_ssize_t {
        auto* ctx = static_cast<OciLayerContext*>(client_data);

        // 1. Hash the raw (uncompressed) tar bytes for diff_id
        if (EVP_DigestUpdate(ctx->diff_ctx, buffer, length) != 1) {
                return -1;
        }

        // 2. Compress through zlib gzip and hash + write compressed output
        ctx->zstrm.next_in = static_cast<unsigned char*>(const_cast<void*>(buffer));
        ctx->zstrm.avail_in = static_cast<unsigned int>(length);

        while (ctx->zstrm.avail_in > 0) {
                ctx->zstrm.next_out = ctx->zbuf.data();
                ctx->zstrm.avail_out = static_cast<unsigned int>(ctx->zbuf.size());

                int ret = deflate(&ctx->zstrm, Z_NO_FLUSH);
                if (ret == Z_STREAM_ERROR) {
                        return -1;
                }

                size_t have = ctx->zbuf.size() - ctx->zstrm.avail_out;
                if (have > 0) {
                        // Hash compressed bytes for blob_digest
                        if (EVP_DigestUpdate(ctx->blob_ctx, ctx->zbuf.data(), have) != 1) {
                                return -1;
                        }
                        // Write compressed bytes to output file
                        if (std::fwrite(ctx->zbuf.data(), 1, have, ctx->output_fp) != have) {
                                return -1;
                        }
                        ctx->compressed_size += have;
                }
        }

        return static_cast<la_ssize_t>(length);
}

auto oci_layer_open_cb(struct archive* /*ar*/, void* /*client_data*/) -> int {
        return ARCHIVE_OK;
}

auto oci_layer_close_cb(struct archive* /*ar*/, void* /*client_data*/) -> int {
        return ARCHIVE_OK;
}

// Collect filesystem entries from the upper directory, sorted lexicographically
struct FsEntry {
        fs::path absolute_path;
        fs::path relative_path;
        bool is_whiteout{false};
        bool is_opaque{false};
};

auto collect_entries(const fs::path& upper_dir) -> std::vector<FsEntry> {
        std::vector<FsEntry> entries{};

        for (const auto& entry : fs::recursive_directory_iterator(
                        upper_dir, fs::directory_options::skip_permission_denied)) {
                // Use lexically_relative to avoid resolving symlinks
                fs::path rel = entry.path().lexically_relative(upper_dir);
                FsEntry fe{};
                fe.absolute_path = entry.path();
                fe.relative_path = rel;
                fe.is_whiteout = Utils::is_overlay_whiteout(entry.path());
                // Only directories can be opaque
                if (entry.is_directory()) {
                        fe.is_opaque = Utils::is_opaque_directory(entry.path());
                }
                entries.push_back(std::move(fe));
        }

        // Sort lexicographically by relative path for reproducible builds
        std::sort(entries.begin(), entries.end(),
                  [](const FsEntry& a, const FsEntry& b) {
                          return a.relative_path < b.relative_path;
                  });

        return entries;
}

// Emit a single empty regular file entry into the tar archive
auto emit_empty_file(struct archive* ar, const std::string& pathname) -> void {
        archive_entry* ae = archive_entry_new();
        if (!ae) {
                throw std::runtime_error("OCI Layer Error: archive_entry_new() failed");
        }

        archive_entry_set_pathname(ae, pathname.c_str());
        archive_entry_set_filetype(ae, AE_IFREG);
        archive_entry_set_size(ae, 0);
        archive_entry_set_perm(ae, 0644);
        // Reproducible metadata
        archive_entry_set_uid(ae, 0);
        archive_entry_set_gid(ae, 0);
        archive_entry_set_uname(ae, "");
        archive_entry_set_gname(ae, "");
        archive_entry_set_mtime(ae, 0, 0);

        if (archive_write_header(ar, ae) != ARCHIVE_OK) {
                archive_entry_free(ae);
                throw std::runtime_error(std::format(
                        "OCI Layer Error: failed to write header for '{}' - {}",
                        pathname, archive_error_string(ar)));
        }
        archive_entry_free(ae);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// create_oci_layer — fully OCI-compliant layer exporter
// ---------------------------------------------------------------------------

auto Utils::create_oci_layer(
        const fs::path& upper_dir,
        const fs::path& output_path
) -> LayerInfo {
        // --- RAII wrappers ---

        // EVP_MD_CTX for diff_id (uncompressed tar hash)
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>
                diff_ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
        if (!diff_ctx) {
                throw std::runtime_error("OCI Layer Error: failed to create diff_id EVP context");
        }
        if (EVP_DigestInit_ex(diff_ctx.get(), EVP_sha256(), nullptr) != 1) {
                throw std::runtime_error("OCI Layer Error: EVP_DigestInit_ex failed for diff_id");
        }

        // EVP_MD_CTX for blob_digest (compressed blob hash)
        std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>
                blob_ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
        if (!blob_ctx) {
                throw std::runtime_error("OCI Layer Error: failed to create blob_digest EVP context");
        }
        if (EVP_DigestInit_ex(blob_ctx.get(), EVP_sha256(), nullptr) != 1) {
                throw std::runtime_error("OCI Layer Error: EVP_DigestInit_ex failed for blob_digest");
        }

        // Ensure parent directory of output exists
        if (auto parent = output_path.parent_path(); !parent.empty()) {
                ensure_dir(parent);
        }

        // Open output file
        FILE* output_fp = std::fopen(output_path.c_str(), "wb");
        if (!output_fp) {
                throw std::runtime_error(std::format(
                        "OCI Layer Error: failed to open output file '{}' - {}",
                        output_path.string(), std::strerror(errno)));
        }
        // RAII guard for fclose
        auto file_guard = std::unique_ptr<FILE, decltype(&std::fclose)>(output_fp, std::fclose);

        // Initialize the write callback context
        OciLayerContext ctx{diff_ctx.get(), blob_ctx.get(), output_fp};

        // Initialize zlib for gzip compression (windowBits = 15 + 16 for gzip)
        ctx.zstrm.zalloc = Z_NULL;
        ctx.zstrm.zfree = Z_NULL;
        ctx.zstrm.opaque = Z_NULL;
        int zret = deflateInit2(&ctx.zstrm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                                15 + 16, 8, Z_DEFAULT_STRATEGY);
        if (zret != Z_OK) {
                throw std::runtime_error("OCI Layer Error: deflateInit2 failed");
        }
        // RAII guard for deflateEnd
        auto zlib_guard = std::unique_ptr<z_stream, decltype(+[](z_stream* s) { deflateEnd(s); })>(
                &ctx.zstrm, +[](z_stream* s) { deflateEnd(s); });

        // --- Create the libarchive writer with custom callbacks ---
        struct archive* ar = archive_write_new();
        if (!ar) {
                throw std::runtime_error("OCI Layer Error: archive_write_new() failed");
        }
        // RAII guard for archive
        auto archive_guard = std::unique_ptr<struct archive, decltype(+[](struct archive* a) {
                archive_write_free(a);
        })>(ar, +[](struct archive* a) {
                archive_write_free(a);
        });

        archive_write_set_format_pax_restricted(ar);
        // NO gzip filter — we do gzip ourselves in the callback

        if (archive_write_open(ar, &ctx, oci_layer_open_cb,
                               oci_layer_write_cb, oci_layer_close_cb) != ARCHIVE_OK) {
                throw std::runtime_error(std::format(
                        "OCI Layer Error: archive_write_open failed - {}",
                        archive_error_string(ar)));
        }

        // --- Collect and sort entries ---
        auto entries = collect_entries(upper_dir);

        // --- Stream entries into the tar ---
        char buffer[16384];

        for (const auto& fe : entries) {
                // OverlayFS kernel whiteout → OCI .wh.<name> entry
                if (fe.is_whiteout) {
                        std::string wh_name = ".wh." + fe.relative_path.filename().string();
                        fs::path wh_path = fe.relative_path.parent_path() / wh_name;
                        emit_empty_file(ar, wh_path.generic_string());
                        continue;
                }

                // Stat the file (lstat to not follow symlinks)
                struct stat st{};
                if (lstat(fe.absolute_path.c_str(), &st) != 0) {
                        throw std::runtime_error(std::format(
                                "OCI Layer Error: lstat failed for '{}' - {}",
                                fe.absolute_path.string(), std::strerror(errno)));
                }

                archive_entry* ae = archive_entry_new();
                if (!ae) {
                        throw std::runtime_error("OCI Layer Error: archive_entry_new() failed");
                }

                // Set path with trailing slash for directories
                std::string entry_path = fe.relative_path.generic_string();
                if (S_ISDIR(st.st_mode) && !entry_path.empty() && entry_path.back() != '/') {
                        entry_path += '/';
                }
                archive_entry_set_pathname(ae, entry_path.c_str());

                // Set file type and permissions from stat
                archive_entry_set_perm(ae, st.st_mode & 07777);

                if (S_ISDIR(st.st_mode)) {
                        archive_entry_set_filetype(ae, AE_IFDIR);
                        archive_entry_set_size(ae, 0);
                } else if (S_ISLNK(st.st_mode)) {
                        archive_entry_set_filetype(ae, AE_IFLNK);
                        // Read symlink target
                        auto target = fs::read_symlink(fe.absolute_path);
                        archive_entry_set_symlink(ae, target.generic_string().c_str());
                        archive_entry_set_size(ae, 0);
                } else if (S_ISREG(st.st_mode)) {
                        archive_entry_set_filetype(ae, AE_IFREG);
                        archive_entry_set_size(ae, st.st_size);
                } else {
                        // Skip other types (block devices, pipes, sockets, etc.)
                        archive_entry_free(ae);
                        continue;
                }

                // Reproducible metadata normalization (BuildKit compatible)
                archive_entry_set_uid(ae, 0);
                archive_entry_set_gid(ae, 0);
                archive_entry_set_uname(ae, "");
                archive_entry_set_gname(ae, "");
                archive_entry_set_mtime(ae, 0, 0);

                if (archive_write_header(ar, ae) != ARCHIVE_OK) {
                        archive_entry_free(ae);
                        throw std::runtime_error(std::format(
                                "OCI Layer Error: failed to write header for '{}' - {}",
                                entry_path, archive_error_string(ar)));
                }

                // Stream regular file contents
                if (S_ISREG(st.st_mode) && st.st_size > 0) {
                        int fd = open(fe.absolute_path.c_str(), O_RDONLY);
                        if (fd < 0) {
                                archive_entry_free(ae);
                                throw std::runtime_error(std::format(
                                        "OCI Layer Error: failed to open '{}' for reading - {}",
                                        fe.absolute_path.string(), std::strerror(errno)));
                        }
                        // RAII guard for fd
                        auto fd_guard = std::unique_ptr<int, decltype(+[](int* p) { close(*p); })>(
                                &fd, +[](int* p) { close(*p); });

                        ssize_t n{};
                        while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
                                if (archive_write_data(ar, buffer, static_cast<size_t>(n))
                                    < static_cast<la_ssize_t>(n)) {
                                        archive_entry_free(ae);
                                        throw std::runtime_error(std::format(
                                                "OCI Layer Error: archive_write_data failed for '{}'",
                                                fe.absolute_path.string()));
                                }
                        }
                        if (n < 0) {
                                archive_entry_free(ae);
                                throw std::runtime_error(std::format(
                                        "OCI Layer Error: read failed for '{}' - {}",
                                        fe.absolute_path.string(), std::strerror(errno)));
                        }
                }

                // Emit .wh..wh..opq for opaque directories (after the dir entry itself)
                if (S_ISDIR(st.st_mode) && fe.is_opaque) {
                        std::string opq_path = fe.relative_path.generic_string();
                        if (!opq_path.empty() && opq_path.back() != '/') {
                                opq_path += '/';
                        }
                        opq_path += ".wh..wh..opq";
                        emit_empty_file(ar, opq_path);
                }

                archive_entry_free(ae);
        }

        // --- Close the archive (emits EOF blocks through callback) ---
        if (archive_write_close(ar) != ARCHIVE_OK) {
                throw std::runtime_error(std::format(
                        "OCI Layer Error: archive_write_close failed - {}",
                        archive_error_string(ar)));
        }

        // --- Finalize gzip stream ---
        ctx.zstrm.avail_in = 0;
        ctx.zstrm.next_in = nullptr;
        int flush_ret{};
        do {
                ctx.zstrm.next_out = ctx.zbuf.data();
                ctx.zstrm.avail_out = static_cast<unsigned int>(ctx.zbuf.size());

                flush_ret = deflate(&ctx.zstrm, Z_FINISH);
                if (flush_ret == Z_STREAM_ERROR) {
                        throw std::runtime_error("OCI Layer Error: deflate Z_FINISH failed");
                }

                size_t have = ctx.zbuf.size() - ctx.zstrm.avail_out;
                if (have > 0) {
                        if (EVP_DigestUpdate(blob_ctx.get(), ctx.zbuf.data(), have) != 1) {
                                throw std::runtime_error(
                                        "OCI Layer Error: EVP_DigestUpdate failed during gzip finalization");
                        }
                        if (std::fwrite(ctx.zbuf.data(), 1, have, output_fp) != have) {
                                throw std::runtime_error(
                                        "OCI Layer Error: fwrite failed during gzip finalization");
                        }
                        ctx.compressed_size += have;
                }
        } while (flush_ret != Z_STREAM_END);

        // Flush the output file
        if (std::fflush(output_fp) != 0) {
                throw std::runtime_error(std::format(
                        "OCI Layer Error: fflush failed - {}", std::strerror(errno)));
        }

        // --- Compute final digests ---
        LayerInfo info{};
        info.diff_id = "sha256:" + sha256_final(diff_ctx.get());
        info.blob_digest = "sha256:" + sha256_final(blob_ctx.get());
        info.blob_size = ctx.compressed_size;

        return info;
}
