#include "../include/utils.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <openssl/sha.h>
#include <random>
#include <chrono>

bool Utils::path_exists(const std::string& path){
    struct stat st{};
    return stat(path.c_str(), &st) == 0;
}

void Utils::ensure_dirs(const std::string& path, const mode_t& mode){
    if (path.empty()) handle_error("Empty path");
    std::string partial{};
    if (path[0] == '/')
        partial = "/";
    size_t start{ 0 };
    while (start < path.size()) {
        size_t pos { path.find('/', start) };
        std::string dir { (pos == std::string::npos) ? path : path.substr(0, pos) };
        if (!dir.empty() && dir != "/") {
            if (mkdir(dir.c_str(), mode) == ERR) {
                if (errno != EEXIST) {
                    handle_error("Unable to create " + dir);
                }
            }
        }
        if (pos == std::string::npos) break;
        start = pos + 1;
    }
}

void Utils::handle_error(const std::string& err){
    std::cerr << "Error: " << err << '\n';
    exit(EXIT_FAILURE);
}

void Utils::write_file(const std::string& path,const std::string& buffer){
    int fd{ open(path.c_str(), O_WRONLY) };
    if(fd == ERR) handle_error("Bad file descriptor for " + path);
    if(write(fd, buffer.c_str(), buffer.length()) == ERR){
        close(fd);
        handle_error("Unable to write to file " + path);
    }
    close(fd);
}

std::string Utils::get_base_dir(){
    const char* home{ getenv("HOME") };
    std::string base{ home ? std::string(home) : "/tmp" };
    return base + "/.quiver";
}
std::string Utils::get_sock_path(const pid_t& pid){
    std::string path{ get_base_dir() + "/containers/" + std::to_string(static_cast<long long>(pid)) };
    ensure_dirs(path);
    return path + "/attach.sock";
}

std::string Utils::get_filesystem_path(const pid_t& pid){
    std::string path{ get_base_dir() + "/filesystems/" + std::to_string(static_cast<long long>(pid)) };
    ensure_dirs(path);
    return path;
}

std::string Utils::get_vfs_path(const pid_t &pid){
    std::string path{ get_base_dir() + "/vfs/" + std::to_string(static_cast<long long>(pid)) };
    return path;
}

std::string Utils::get_image_path(const std::string& image_name){
    std::string path{ get_base_dir() + "/images/" + image_name};
    ensure_dirs(path);
    return path;
}

std::string Utils::get_logs_path(const pid_t& pid){
    std::string path{ get_base_dir() + "/logs/" + std::to_string(static_cast<long long>(pid)) };
    ensure_dirs(path);
    return path;
}

void Utils::print_usage(){
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

              << "  pull <image_name>                Pull an image from a registry\n"
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
              << "                  <host:cont> ...  Create a volume link for container\n"

              << "  help                             Show this help message\n";
}

std::string Utils::generate_container_id() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::random_device rd;
    std::string input = std::to_string(now) + ":" + std::to_string(rd());

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);

    std::string hexString;
    hexString.reserve(SHA256_DIGEST_LENGTH * 2);
    static const char hexDigits[] = "0123456789abcdef";

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hexString += hexDigits[(hash[i] >> 4) & 0xF];
        hexString += hexDigits[hash[i] & 0xF];
    }

    return hexString;
}
