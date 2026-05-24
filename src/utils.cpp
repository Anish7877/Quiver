#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <format>
#include <random>
#include <blake3.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <cstring>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <pwd.h>
#include <archive.h>
#include <set>
#include <unistd.h>
#include "utils.hpp"
#include "oci_runtime.hpp"
#include "log_job_processor.hpp"
#include "types.hpp"


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

auto Utils::get_base_dir() -> fs::path {
        const char* home{getenv("HOME")};
        std::string base{home ? std::string(home) : "/tmp"};
        return base + "/.quiver";
}

auto Utils::get_sock_path(const std::string& container_id) -> fs::path {
        std::string path{std::format("/tmp/quiver_{}.sock", container_id)};
        return path;
}

auto Utils::get_filesystem_path(const std::string& container_id) -> fs::path {
        std::string path{std::format("{}/filesystems/{}",get_base_dir().string(), container_id)};
        return path;
}

auto Utils::get_vfs_path(const std::string& container_id) -> fs::path {
        std::string path{std::format("{}/vfs/{}",get_base_dir().string(), container_id)};
        return path;
}

auto Utils::get_image_path(const std::string& image_name) -> fs::path {
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


auto Utils::extract_tarball(const std::string& tarball_path, const std::string& destination_path) -> void {
        pid_t pid{fork()};

        if (pid == -1) [[unlikely]] {
                throw std::runtime_error("Tar Error: fork failed");
        }
        else if (pid == 0) {
                execlp("tar", "tar", "-xzf", tarball_path.c_str(), "-C", destination_path.c_str(), NULL);
                std::cerr << "Tar Error: execlp failed\n";
                _exit(EXIT_FAILURE);
        }
        else {
                int status{};
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                        int exit_code{WEXITSTATUS(status)};
                        if (exit_code > 0) [[unlikely]] {
                                std::cerr << std::format("Tar Error: exited with error code: {}\n", exit_code);\
                                _exit(EXIT_FAILURE);
                        }
                }
        }
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
        log_job_processor.process_job();

        pid_t my_pid{getpid()};
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

auto Utils::print_usage() -> void {
        std::cout << "Usage: quiver <command> [options] [arguments]\n\n"
                << "Commands:\n"
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
