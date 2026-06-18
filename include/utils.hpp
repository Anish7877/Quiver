#pragma once
#include "oci_runtime.hpp"
#include "types.hpp"
#include <filesystem>
#include <string>
namespace fs = std::filesystem;

class LoggerCommandQueue;
class ValueHeap;
namespace Utils {
        inline std::atomic<bool> job_processor_running{true};
        auto dir_exists(const fs::path&) -> bool;
        auto file_exists(const fs::path&) -> bool;
        auto ensure_dir(const fs::path&, mode_t mode = 0755) -> void;
        auto ensure_file(const fs::path&) -> void;
        auto write_file(const fs::path&, std::string_view, bool append_mode = false) -> void;
        auto copy_directory(const fs::path&, const fs::path&) -> void;
        auto remove_directory(const fs::path&) -> void;
        auto get_base_dir() -> fs::path;
        auto get_sock_path(const std::string&) -> fs::path;
        auto get_filesystem_path(const std::string&) -> fs::path;
        auto get_vfs_path(const std::string&) -> fs::path;
        auto get_image_path(const std::string&) -> fs::path;
        auto get_db_path(std::string_view) -> fs::path;
        auto get_log_path(std::string_view) -> fs::path;
        auto get_logger_command_queue_buf_name() -> std::string;
        auto get_database_command_queue_buf_name() -> std::string;
        auto get_value_heap_buf_name() -> std::string;
        auto get_device_gid(const fs::path&) -> gid_t;
        auto get_gid_map_payload(const std::vector<OCIRuntime::Device>&) -> std::string;
        auto find_program_path(const std::string&) -> fs::path;
        auto generate_container_id() -> std::string;
        auto extract_tarball(const std::string&, const std::string&) -> void;
        auto spawn_new_consumer() -> pid_t;
        auto parse_subgid(const std::string&) -> std::vector<SubIDRange>;
        auto get_username() -> std::string;
        auto build_gid_map_payload(pid_t) -> std::string;
        auto write_all(int, const char*, ssize_t) -> bool;
        auto print_usage() -> void;
}
