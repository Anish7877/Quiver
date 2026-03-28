#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <format>
#include <random>
#include <blake3.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <archive.h>
#include <set>
#include "utils.hpp"

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
        fs::copy(source, destination, error_code);
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

auto Utils::get_container_db_path() -> fs::path {
        std::string path{std::format("{}/db/container_db", get_base_dir().string())};
        return path;
}

auto Utils::get_volume_db_path() -> fs::path {
        std::string path{std::format("{}/db/volume_db", get_base_dir().string())};
        return path;
}

auto Utils::get_device_db_path() -> fs::path {
        std::string path{std::format("{}/db/device_db", get_base_dir().string())};
        return path;
}

auto Utils::get_network_db_path() -> fs::path {
        std::string path{std::format("{}/db/network_db", get_base_dir().string())};
        return path;
}

auto Utils::get_image_db_path() -> fs::path {
        std::string path{std::format("{}/db/image_db", get_base_dir().string())};
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

auto Utils::get_gid_map_payload(const std::vector<std::pair<fs::path, fs::path>>& devices) -> std::string {
        std::string payload{};
        std::set<gid_t> unique_gids{};

        for (const auto& dev : devices) {
                gid_t gid{get_device_gid(dev.first)};

                if (unique_gids.insert(gid).second) {
                        payload += std::format("{} {} 1\n", gid, gid);
                }
        }
        return payload;
}

auto Utils::generate_container_id() -> std::string {
        auto now{std::chrono::high_resolution_clock::now().time_since_epoch().count()};
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
