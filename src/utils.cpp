#include "../include/utils.hpp"
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <iostream>
#include <unistd.h>

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
    std::string path{ get_base_dir() + "containers/" + std::to_string(static_cast<long long>(pid)) + "/attach.sock" };
    return path;
}

std::string Utils::get_filesystem_path(const pid_t& pid){
    std::string path{ get_base_dir() + "filesystems/" + std::to_string(static_cast<long long>(pid)) };
    return path;
}

std::string Utils::get_image_path(const std::string& image_name){
    std::string path{ get_base_dir() + "images/" + image_name};
    return path;
}
