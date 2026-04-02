#pragma once
#include <filesystem>
#include <string>
namespace fs = std::filesystem;

class LoggerCommandQueue;
class ValueHeap;
namespace Utils {
    auto dir_exists(const fs::path&) -> bool;
    auto file_exists(const fs::path&) -> bool;
    auto ensure_dir(const fs::path&, mode_t mode = 0755) -> bool;
    auto ensure_file(const fs::path&) -> bool;
    auto write_file(const fs::path&, std::string_view, bool append_mode = false) -> bool;
    auto copy_directory(const fs::path&, const fs::path&) -> bool;
    auto remove_directory(const fs::path&) -> bool;
    auto get_base_dir() -> fs::path;
    auto get_sock_path(pid_t) -> fs::path;
    auto get_filesystem_path(pid_t) -> fs::path;
    auto get_vfs_path(pid_t) -> fs::path;
    auto get_image_path(const std::string&) -> fs::path;
    auto get_container_db_path() -> fs::path;
    auto get_volume_db_path() -> fs::path;
    auto get_device_db_path() -> fs::path;
    auto get_network_db_path() -> fs::path;
    auto get_image_db_path() -> fs::path;
    auto get_logger_command_queue_buf_name() -> std::string;
    auto get_database_command_queue_buf_name() -> std::string;
    auto get_value_heap_buf_name() -> std::string;
    auto get_device_gid(const std::string&) -> gid_t;
    auto generate_container_id() -> std::string;
    auto extract_tarball(const std::string&, const std::string&) -> void;
    auto print_usage() -> void;
    auto sha256(std::string_view data) -> std::string;
}
