#include <filesystem>
#include <iostream>
#include <fstream>
#include <format>
#include <random>
#include "utils.hpp"

auto Utils::dir_exists(const fs::path& path) -> bool {
        return fs::is_directory(path);
}

auto Utils::file_exists(const fs::path& path) -> bool {
        return fs::is_regular_file(path);
}

auto Utils::ensure_dir(const fs::path& path, mode_t mode) -> void {
        if(!dir_exists(path)) {
                std::error_code error_code{};
                fs::create_directories(path, error_code);

                if (error_code) [[unlikely]] {
                        throw std::runtime_error(std::format("Directory Error: couldn't create '{}' - {}", path.string(), error_code.message()));
                }

                fs::permissions(path, static_cast<fs::perms>(mode), fs::perm_options::replace, error_code);
                if (error_code) [[unlikely]] {
                        throw std::runtime_error(std::format("Permissions Error: couldn't set permissions for '{}' - {}", path.string(), error_code.message()));
                }
        }
}

auto Utils::ensure_file(const fs::path& path) -> void {
        if(!file_exists(path)) {
                fs::path parent_path{path.parent_path()};
                if(!parent_path.empty() && !dir_exists(parent_path)) ensure_dir(parent_path);
                std::ofstream file{path};
                if(!file) [[unlikely]] {
                        throw std::runtime_error(std::format("File Error: failed to create '{}'", path.string()));
                }
        }
}

auto Utils::write_file(const fs::path& path, std::string_view buffer, bool append_mode) -> void {
        fs::path parent_path{path.parent_path()};
        if(!parent_path.empty() && !dir_exists(parent_path)) ensure_dir(parent_path);

        std::ios_base::openmode mode{std::ios::out};
        if (append_mode) {
                mode |= std::ios::app;
        }
        std::ofstream file{path, mode};

        if (!file.is_open()) [[unlikely]] {
                throw std::runtime_error(std::format("File Error: couldn't open '{}'", path.string()));
        }
        file << buffer;
        if (!file) [[unlikely]] {
                throw std::runtime_error(std::format("File Error: failed to write data to '{}'", path.string()));
        }
}

auto Utils::get_base_dir() -> fs::path {
        const char* home{getenv("HOME")};
        std::string base{home ? std::string(home) : "/tmp"};
        return base + "/.quiver";
}

auto Utils::get_sock_path(pid_t pid) -> fs::path {
        std::string path{get_base_dir().string() + "/containers/" + std::to_string(static_cast<long long>(pid))};
        return path;
}

auto Utils::get_filesystem_path(pid_t pid) -> fs::path {
        std::string path{get_base_dir().string() + "/filesystems/" + std::to_string(static_cast<long long>(pid))};
        return path;
}

auto Utils::get_vfs_path(pid_t pid) -> fs::path {
        std::string path{get_base_dir().string() + "/vfs/" + std::to_string(static_cast<long long>(pid))};
        return path;
}

auto Utils::get_image_path(const std::string& image_name) -> fs::path {
        std::string path{get_base_dir().string() + "/images/" + image_name};
        return path;
}

auto Utils::get_logs_path(pid_t pid) -> fs::path {
        std::string path{get_base_dir().string() + "/logs/" + std::to_string(static_cast<long long>(pid))};
        return path;
}

auto Utils::get_container_db_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db/container_db"};
        return path;
}

auto Utils::get_volume_db_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db/volume_db"};
        return path;
}

auto Utils::get_device_db_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db/device_db"};
        return path;
}

auto Utils::get_network_db_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db/network_db"};
        return path;
}

auto Utils::get_image_db_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db/image_db"};
        return path;
}

auto Utils::get_container_db_log_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db_logs/container_db.log"};
        return path;
}

auto Utils::get_volume_db_log_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db_logs/volume_db.log"};
        return path;
}

auto Utils::get_device_db_log_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db_logs/device_db.log"};
        return path;
}

auto Utils::get_network_db_log_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db_logs/network_db.log"};
        return path;
}

auto Utils::get_image_db_log_path() -> fs::path {
        std::string path{get_base_dir().string() + "/db_logs/image_db.log"};
        return path;
}

auto Utils::generate_container_id() -> std::string {
        auto now{std::chrono::high_resolution_clock::now().time_since_epoch().count()};
        std::random_device rd{};
        std::string input{std::to_string(now) + ":" + std::to_string(rd())};

        unsigned char hash[SHA256_DIGEST_LENGTH]{};
        SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

        std::string hexString{};
        hexString.reserve(SHA256_DIGEST_LENGTH * 2);
        static const char hexDigits[]{"0123456789abcdef"};

        for (int i{0}; i < SHA256_DIGEST_LENGTH; i++) {
                hexString += hexDigits[(hash[i] >> 4) & 0xF];
                hexString += hexDigits[hash[i] & 0xF];
        }
        return hexString;
}

auto Utils::remove_directory_recursively(const fs::path& path) -> bool {
        std::error_code error_code{};
        fs::remove_all(path, error_code);
        if(error_code) [[unlikely]] {
                throw std::runtime_error(std::format("Directory Error: couldn't remove '{}' - {}", path.string(), error_code.message()));
        }
        return true;
}

auto Utils::extract_tarball(const std::string& tarball_path, const std::string& destination_path) -> void {
        pid_t pid{fork()};

        if (pid == CERR) [[unlikely]] {
                throw std::runtime_error("Tar Error: fork failed");
        }
        else if (pid == 0) {
                execlp("tar", "tar", "-xzf", tarball_path.c_str(), "-C", destination_path.c_str(), NULL);
                throw std::runtime_error("Tar Error: execlp failed");
                exit(CERR);
        }
        else {
                int status{};
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                        int exit_code{WEXITSTATUS(status)};
                        if (exit_code != 0) [[unlikely]] {
                                throw std::runtime_error(std::format("Tar Error: exited with error code: {}", exit_code));
                        }
                }
        }
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
