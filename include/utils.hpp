#pragma once
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <openssl/sha.h>
namespace fs = std::filesystem;

namespace Utils {
    auto dir_exists(const fs::path& path) -> bool;
    auto file_exists(const fs::path& path) -> bool;
    auto ensure_dir(const fs::path& path, mode_t mode = 0755) -> void;
    auto ensure_file(const fs::path& path) -> void;
    auto write_file(const fs::path& path, std::string_view buffer, bool append_mode = false) -> void;
    auto get_base_dir() -> fs::path;
    auto get_sock_path(pid_t pid) -> fs::path;
    auto get_filesystem_path(pid_t pid) -> fs::path;
    auto get_vfs_path(pid_t pid) -> fs::path;
    auto get_image_path(const std::string& image_name) -> fs::path;
    auto get_container_db_path() -> fs::path;
    auto get_volume_db_path() -> fs::path;
    auto get_device_db_path() -> fs::path;
    auto get_network_db_path() -> fs::path;
    auto get_image_db_path() -> fs::path;
    auto get_logger_command_queue_buf_name() -> std::string;
    auto get_database_command_queue_buf_name() -> std::string;
    auto get_value_heap_buf_name() -> std::string;
    auto generate_container_id() -> std::string;
    auto remove_directory_recursively(const fs::path& path) -> bool;
    auto extract_tarball(const std::string& tarball_path, const std::string& destination_path) -> void;
    auto print_usage() -> void;
}
