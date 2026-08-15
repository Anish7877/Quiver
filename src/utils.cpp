#include "log_job_processor.hpp"
#include "oci_runtime.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <array>
#include <atomic>
#include <blake3.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <ostream>
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
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <zlib.h>
#include <nlohmann/json.hpp>
#include "database_job_processor.hpp"
using json = nlohmann::json;

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
        if (fs::exists(path)) {
                fs::permissions(path, fs::perms::all, fs::perm_options::add, error_code);
                for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied)) {
                        fs::permissions(entry.path(), fs::perms::all, fs::perm_options::add, error_code);
                }
        }
        fs::remove_all(path, error_code);
        if (error_code == std::errc::permission_denied) {
                pid_t pid{fork()};
                if (pid == 0) {
                        if (unshare(CLONE_NEWUSER) == 0) {
                                std::error_code ec{};
                                fs::remove_all(path, ec);
                        }
                        _exit(0);
                } else if (pid > 0) {
                        waitpid(pid, nullptr, 0);
                        error_code.clear();
                }
        }
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

auto Utils::sanitize_image_name(std::string_view image_name) -> std::string {
        std::string safe_name{image_name};
        std::replace(safe_name.begin(), safe_name.end(), ':', '_');
        std::replace(safe_name.begin(), safe_name.end(), '/', '_');
        return safe_name;
}

auto Utils::get_image_path(std::string_view image_name) -> fs::path {
        std::string path{std::format("{}/images/{}", get_base_dir().string(), sanitize_image_name(image_name))};
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
        return "/log_command_queue";
}

auto Utils::get_database_command_queue_buf_name() -> std::string {
        return "/db_command_queue";
}

auto Utils::get_value_heap_buf_name() -> std::string {
        return "/value_heap";
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
        std::ofstream job_processor_log{get_log_path("log_processor"), std::ios::app};
        auto& log_job_processor{LogJobProcessor::get_instance()};
        auto& database_job_processor{DatabaseJobProcessor::get_instance()};
        try {
                log_job_processor.init();
                database_job_processor.init();
        }
        catch (const std::exception& e) {
                job_processor_log << e.what() << '\n' << std::flush;
        }
        pid_t my_pid{getpid()};
        log_job_processor.process_job();
        database_job_processor.process_job();
        if (write(sync_pipe[1], &my_pid, sizeof(my_pid)) != static_cast<ssize_t>(sizeof(my_pid))) {
                close(sync_pipe[1]);
                _exit(EXIT_FAILURE);
        }
        close(sync_pipe[1]);

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

auto Utils::resolve_user_group(const std::vector<std::string>& lower_dirs, const std::string& spec) -> std::pair<uid_t, gid_t> {
        auto is_number{[](std::string_view s) -> bool {
                return !s.empty() && std::all_of(s.begin(), s.end(),
                                [](unsigned char c) -> bool{ return std::isdigit(c); });
        }};

        size_t colon_index{spec.find(':')};
        std::string user = (colon_index == std::string::npos) ? spec : spec.substr(0, colon_index);
        std::string group = (colon_index == std::string::npos) ? "" : spec.substr(colon_index + 1);

        uid_t uid = 0;
        gid_t gid = 0;
        bool uid_set = false;
        bool gid_set = false;

        if (is_number(user)) {
                uid = static_cast<uid_t>(std::stoul(user));
                uid_set = true;
                if (group.empty()) {
                        gid = static_cast<gid_t>(uid);
                        gid_set = true;
                }
        }

        if (!group.empty() && is_number(group)) {
                gid = static_cast<gid_t>(std::stoul(group));
                gid_set = true;
        }

        if (!uid_set || !gid_set) {
                // Find /etc/passwd and /etc/group from lower_dirs (top to bottom)
                fs::path passwd_file;
                fs::path group_file;

                for (auto it = lower_dirs.rbegin(); it != lower_dirs.rend(); ++it) {
                        const auto& dir = *it;
                        if (passwd_file.empty() && fs::exists(fs::path(dir) / "etc" / "passwd")) {
                                passwd_file = fs::path(dir) / "etc" / "passwd";
                        }
                        if (group_file.empty() && fs::exists(fs::path(dir) / "etc" / "group")) {
                                group_file = fs::path(dir) / "etc" / "group";
                        }
                        if (!passwd_file.empty() && !group_file.empty()) break;
                }

                if (!uid_set && !passwd_file.empty()) {
                        std::ifstream ifs(passwd_file);
                        std::string line;
                        while (std::getline(ifs, line)) {
                                size_t p1 = line.find(':');
                                if (p1 == std::string::npos) continue;
                                if (line.substr(0, p1) == user) {
                                        size_t p2 = line.find(':', p1 + 1);
                                        size_t p3 = line.find(':', p2 + 1);
                                        size_t p4 = line.find(':', p3 + 1);
                                        if (p3 != std::string::npos && p4 != std::string::npos) {
                                                uid = static_cast<uid_t>(std::stoul(line.substr(p2 + 1, p3 - p2 - 1)));
                                                if (group.empty()) {
                                                        gid = static_cast<gid_t>(std::stoul(line.substr(p3 + 1, p4 - p3 - 1)));
                                                        gid_set = true;
                                                }
                                                uid_set = true;
                                        }
                                        break;
                                }
                        }
                }

                if (!gid_set && !group.empty() && fs::exists(group_file)) {
                        std::ifstream ifs(group_file);
                        std::string line;
                        while (std::getline(ifs, line)) {
                                size_t p1 = line.find(':');
                                if (p1 == std::string::npos) continue;
                                if (line.substr(0, p1) == group) {
                                        size_t p2 = line.find(':', p1 + 1);
                                        size_t p3 = line.find(':', p2 + 1);
                                        if (p2 != std::string::npos && p3 != std::string::npos) {
                                                gid = static_cast<gid_t>(std::stoul(line.substr(p2 + 1, p3 - p2 - 1)));
                                                gid_set = true;
                                        }
                                        break;
                                }
                        }
                }
        }

        if (!uid_set && !is_number(user)) {
                // throw std::runtime_error(std::format("Unknown user '{}'", user));
                // Usually Docker defaults to 0 if not found, or fails. We fail to match builder.
                throw std::runtime_error(std::format("Unknown user '{}'", user));
        }
        if (!gid_set && !group.empty() && !is_number(group)) {
                throw std::runtime_error(std::format("Unknown group '{}'", group));
        }

        return {uid, gid};
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

auto Utils::write_all(int fd, const char* buf, size_t len) -> bool {
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

auto Utils::extract_tarball(const std::string& tarball_path, const std::string& destination_path) -> void {
        struct archive* a;
        struct archive* ext;
        struct archive_entry* entry;

        // FIX: Removed ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS since we are manually
        // sanitizing and providing safe absolute paths to libarchive.
        int flags{ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL |
                  ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_SECURE_SYMLINKS};
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

auto Utils::extract_oci_layer(const std::string& tarball_path, const std::string& destination_path) -> void {
        struct archive* a;
        struct archive* ext;
        struct archive_entry* entry;

        // Standard extraction flags: preserve time, permissions, ACLs, flags, and security checks
        int flags{ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS |
                  ARCHIVE_EXTRACT_SECURE_SYMLINKS};
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

                // OCI Whiteout Handling
                std::string filename = entry_path.filename().string();
                if (filename == ".wh..wh..opq") {
                        fs::path parent_dir = canonical_dest / entry_path.parent_path();
                        if (fs::exists(parent_dir) && fs::is_directory(parent_dir)) {
                                for (auto& p : fs::directory_iterator(parent_dir)) {
                                        fs::remove_all(p.path());
                                }
                        }
                        // Skip writing the .wh..wh..opq file
                        continue;
                } else if (filename.starts_with(".wh.")) {
                        std::string target = filename.substr(4);
                        fs::path target_path = canonical_dest / entry_path.parent_path() / target;
                        if (fs::exists(target_path)) {
                                fs::remove_all(target_path);
                        }
                        // Skip writing the whiteout file
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


auto PrintUtils::print_section(std::string_view name) -> void {
        std::cout << "\n";
        std::cout << name << '\n';
        std::cout << std::string(name.size(), '=') << '\n';
}

auto PrintUtils::print_container_config(const ContainerConfig& c) -> void {
        print_section("General");

        print_field("Container ID", c.container_id);
        print_field("Hostname", c.hostname);
        print_field("Domain Name", c.domain_name);
        print_field("PID", c.pid);
        print_field("Network PID", c.net_pid);
        print_field("VFS", c.vfs);
        print_field("Cgroup Path", c.cgroups_path.string());

        print_section("Root Filesystem");

        print_field("Path", c.rootfs.path.string());
        print_field("Read Only", c.rootfs.read_only);
        print_field("Propagation", c.rootfs_propagation.type);

        print_section("Terminal");

        print_field("Enabled", c.terminal.value);
        print_field("Detach", c.detach.value);
        print_field("PTY", c.pty_slave_name);
        print_field("PTY FD", c.pty_slave_fd);
        print_field("Control Socket", c.control_sock);
        print_field(
                        "Console Size",
                        std::format("{}x{}", c.console_size.width, c.console_size.height));

        print_section("User");

        print_field("UID", c.user.uid);
        print_field("GID", c.user.gid);
        print_field("Umask", c.user.umask);

        print_vector("Additional GIDs", c.user.additional_gids);

        print_field("UID Mapping",
                        std::format("{} -> {} ({})",
                                c.uid_mapping.container_id,
                                c.uid_mapping.host_id,
                                c.uid_mapping.size));

        print_field("GID Mapping",
                        std::format("{} -> {} ({})",
                                c.gid_mapping.container_id,
                                c.gid_mapping.host_id,
                                c.gid_mapping.size));

        print_section("Process");

        print_field("Working Directory", c.cwd.value);
        print_field("OOM Score", c.oom_score.value);
        print_field("No New Privileges", c.no_new_privileges.value);

        print_vector("Arguments", c.args.value);
        print_vector("Environment", c.env.value);

        print_section("Scheduler");

        print_field("Policy", c.schedular_opts.policy);
        print_field("Priority", c.schedular_opts.priority);
        print_field("Nice", c.schedular_opts.nice);
        print_field("Runtime", c.schedular_opts.runtime);
        print_field("Deadline", c.schedular_opts.deadline);
        print_field("Period", c.schedular_opts.period);

        print_vector("Flags", c.schedular_opts.flags);

        print_section("Capabilities");

        print_vector("Bounding", c.capabilities.bounding);
        print_vector("Effective", c.capabilities.effective);
        print_vector("Permitted", c.capabilities.permitted);
        print_vector("Ambient", c.capabilities.ambient);
        print_vector("Inheritable", c.capabilities.inheritable);

        print_section("Resource Limits");

        for (const auto& r : c.rlimits) {
                std::cout << std::format(
                                "  {:<16} soft={} hard={}\n",
                                r.name,
                                r.soft_limit,
                                r.hard_limit);
        }

        print_section("Namespaces");

        for (const auto& ns : c.namespaces) {
                std::cout << std::format(
                                "  {:<10} {}\n",
                                ns.type,
                                ns.path.string());
        }

        print_section("Devices");

        for (const auto& dev : c.devices) {
                std::cout << std::format(
                                "  {} -> {}\n",
                                dev.host_path.string(),
                                dev.container_path.string());
        }

        print_section("Mounts");

        for (const auto& m : c.mounts) {
                std::cout << std::format(
                                "  {} -> {} ({})\n",
                                m.source,
                                m.destination,
                                m.type);

                if (!m.options.empty())
                        print_vector("    Options", m.options);

                if (!m.flags.empty())
                        print_vector("    Flags", m.flags);

                if (!m.attrs.empty())
                        print_vector("    Attrs", m.attrs);
        }

        print_section("Network");

        print_field("Auto TCP", c.networks.auto_tcp);
        print_field("Auto UDP", c.networks.auto_udp);

        print_vector("TCP Ports", c.networks.tcp_ports);
        print_vector("UDP Ports", c.networks.udp_ports);

        print_section("Time Offsets");

        for (const auto& t : c.timeoffsets) {
                std::cout << std::format(
                                "  {:<10} {}s {}ns\n",
                                t.type,
                                t.secs,
                                t.nanosecs);
        }

        print_section("Masked Paths");

        for (const auto& p : c.masked_paths.paths)
                std::cout << std::format("  {}\n", p.string());

        print_section("Read Only Paths");

        for (const auto& p : c.read_only_paths.paths)
                std::cout << std::format("  {}\n", p.string());

        print_section("Seccomp");

        print_field("Default Action", c.seccomp.default_action);
        print_field("Default Errno", c.seccomp.default_errno);

        print_vector("Architectures", c.seccomp.archs);
        print_vector("Flags", c.seccomp.flags);

        std::cout << std::format("Syscall Rules : {}\n",
                        c.seccomp.syscalls.size());
}

auto Utils::load_seccomp_profile(const fs::path& path) -> OCIRuntime::Seccomp {
        std::ifstream file(path);
        if (!file.is_open()) {
                throw std::runtime_error(
                                std::format("Failed to open seccomp profile '{}'",
                                        path.string()));
        }
        json j;
        file >> j;
        OCIRuntime::Seccomp seccomp{};
        seccomp.default_action = j.at("defaultAction").get<std::string>();
        if (j.contains("defaultErrnoRet")) {
                seccomp.default_errno = j["defaultErrnoRet"].get<std::uint32_t>();
        }
        if (j.contains("architectures")) {
                seccomp.archs = j["architectures"].get<std::vector<std::string>>();
        }
        if (j.contains("flags")) {
                seccomp.flags = j["flags"].get<std::vector<std::string>>();
        }
        if (j.contains("syscalls")) {
                for (const auto& syscall_json : j["syscalls"]) {
                        OCIRuntime::Seccomp::SyscallRule rule{};
                        rule.names = syscall_json.at("names").get<std::vector<std::string>>();
                        rule.action = syscall_json.at("action").get<std::string>();

                        if (syscall_json.contains("errnoRet")) {
                                rule.errno_ret = syscall_json["errnoRet"].get<std::uint32_t>();
                        }

                        if (syscall_json.contains("args")) {
                                for (const auto& arg_json : syscall_json["args"]) {
                                        OCIRuntime::Seccomp::Arg arg{};
                                        arg.index = arg_json.at("index").get<std::uint32_t>();
                                        arg.value = arg_json.at("value").get<std::uint64_t>();
                                        if (arg_json.contains("valueTwo")) {
                                                arg.value_two = arg_json["valueTwo"].get<std::uint64_t>();
                                        }
                                        arg.op = arg_json.at("op").get<std::string>();
                                        rule.args.emplace_back(std::move(arg));
                                }
                        }
                        seccomp.syscalls.emplace_back(std::move(rule));
                }
        }
        return seccomp;
}

auto Utils::send_all(int fd, const void* data, size_t size) -> bool {
        const char* ptr{static_cast<const char*>(data)};

        while (size > 0) {
                ssize_t n{send(fd, ptr, size, 0)};
                if (n <= 0) return false;
                ptr += n;
                size -= n;
        }
        return true;
}
auto Utils::recv_all(int fd, void* data, size_t size) -> bool {
        char* ptr{static_cast<char*>(data)};
        while (size > 0) {
                ssize_t n{recv(fd, ptr, size, 0)};
                if (n <= 0) return false;
                ptr += n;
                size -= n;
        }
        return true;
}

auto Utils::create_connection(std::string_view path) -> int {
        if (unlink(path.data()) == -1 && errno != ENOENT) [[unlikely]] {
                return -1;
        }
        int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock_fd == -1) [[unlikely]] {
                return sock_fd;
        }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.data(), sizeof(addr.sun_path)-1);
        if (bind(sock_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) [[unlikely]] {
                return -1;
        }
        if (listen(sock_fd, 1) == -1) [[unlikely]] {
                return -1;
        }
        return sock_fd;
}

auto Utils::is_process_alive(pid_t pid, const std::string& container_id) -> bool {
        auto check_cgroups{[](pid_t pid, const std::string& expected_container_id) -> bool {
                        std::string cgroup_path{"/proc/" + std::to_string(pid) + "/cgroup"};
                        std::string expected_scope{"quiver-" + expected_container_id + ".scope"};

                        int fd{open(cgroup_path.c_str(), O_RDONLY | O_CLOEXEC)};
                        if (fd == -1) {
                                return false;
                        }
                        char buffer[4096];
                        ssize_t bytes_read{read(fd, buffer, sizeof(buffer) - 1)};
                        close(fd);
                        if (bytes_read <= 0) {
                                return false;
                        }
                        buffer[bytes_read] = '\0';
                        if (strstr(buffer, expected_scope.c_str()) != nullptr) {
                                return true;
                        }

                        return false;
                }
        };
        if (kill(pid, 0) == 0) {
                return true;
        }
        if (errno == ESRCH) {
                return false;
        }
        if (errno == EPERM) {
                return true;
        }
        return check_cgroups(pid, container_id);
}

auto Utils::get_boot_time() -> long {
        int fd{open("/proc/stat", O_RDONLY)};
        if (fd < 0) return 0;

        char buf[32768];
        ssize_t bytes_read{read(fd, buf, sizeof(buf))};
        close(fd);

        if (bytes_read <= 0) return 0;

        std::string_view sv(buf, static_cast<size_t>(bytes_read));

        size_t pos{sv.find("\nbtime ")};
        if (pos == std::string_view::npos) {
                if (sv.starts_with("btime ")) pos = 0;
                else return 0;
        } else {
                pos += 1;
        }
        pos += 6;
        long btime{0};
        std::from_chars(sv.data() + pos, sv.data() + sv.size(), btime);

        return btime;
}
