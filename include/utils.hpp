#pragma once
#define ERR -1
#include <string>
#include <filesystem>
namespace fs = std::filesystem;

namespace Utils {
    auto dir_exists(const fs::path& path) -> bool;
    auto path_exists(const fs::path& path) -> bool;
    auto ensure_dirs(const fs::path& path, mode_t mode = 0755) -> void;
    auto ensure_file(const fs::path& path) -> void;
    auto write_file(const fs::path& path, std::string_view buffer) -> void;
    auto handle_error(const std::string&) -> void;
    auto print_usage() -> void;
    auto get_base_dir() -> std::string;
    auto get_sock_path(const pid_t& pid) -> std::string;
    auto get_filesystem_path(const pid_t& pid) -> std::string;
    auto get_vfs_path(const pid_t& pid) -> std::string;
    auto get_image_path(const std::string& image_name) -> std::string;
    auto get_logs_path(const pid_t& pid) -> std::string;
    auto generate_container_id() -> std::string;
    auto remove_directory_recursively(const std::string& path) -> int;
    auto extract_tarball(const std::string& tarball_path, const std::string& destination_path) -> bool;
}
