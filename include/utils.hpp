#pragma once
#define ERR -1
#include <string>
#include <unistd.h>

namespace Utils{
    bool path_exists(const std::string& path);
    void ensure_dirs(const std::string& path,const mode_t& mode = 0755);
    void handle_error(const std::string& err);
    void write_file(const std::string& path,const std::string& buffer);
    void print_usage();
    std::string get_base_dir();
    std::string get_sock_path(const pid_t& pid);
    std::string get_filesystem_path(const pid_t& pid);
    std::string get_vfs_path(const pid_t& pid);
    std::string get_image_path(const std::string& image_name);
    std::string get_logs_path(const pid_t& pid);
    std::string generate_container_id();
    int remove_directory_recursively(const std::string& path);
    bool extract_tarball(const std::string& tarball_path, const std::string& destination_path);
}
