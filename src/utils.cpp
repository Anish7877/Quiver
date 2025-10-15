#include "../include/utils.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
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
    return base + "/.quiver/";
}
std::string Utils::get_sock_path(const pid_t& pid){
    std::string path{ get_base_dir() + "containers/" + std::to_string(static_cast<long long>(pid)) };
    ensure_dirs(path);
    return path + "/attach.sock";
}

std::string Utils::get_filesystem_path(const pid_t& pid){
    std::string path{ get_base_dir() + "filesystems/" + std::to_string(static_cast<long long>(pid)) };
    ensure_dirs(path);
    return path;
}

std::string Utils::get_image_path(const std::string& image_name){
    std::string path{ get_base_dir() + "images/" + image_name};
    ensure_dirs(path);
    return path;
}

void Utils::print_usage(){
    std::cout << "Usage: quiver <command> [options]\n"
                 "Commands:\n"
                 "  run <image> [options]       Run a new container from the specified image\n"
                 "  ps [options]                List containers\n"
                 "  stop <container_id>         Stop a running container\n"
                 "  rm <container_id>           Remove a container\n"
                 "  images                      List available images\n"
                 "  rmi <image_name>            Remove an image\n"
                 "  pull <image_name>           Pull an image from a registry\n"
                 "  exec <container_id> <cmd>   Execute a command in a running container\n"
                 "  attach <container_id>       Attach to a running container's console\n"
                 "  help                        Show this help message\n";
}

std::string Utils::generate_container_id() {

    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::string seed = std::to_string(now);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, seed.c_str(), seed.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}