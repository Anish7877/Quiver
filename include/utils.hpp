#pragma once
#include "oci_runtime.hpp"
#include "types.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <openssl/evp.h>
namespace fs = std::filesystem;

namespace Utils {
        inline std::atomic<bool> job_processor_running{true};

        struct LayerInfo {
                std::string diff_id;
                std::string blob_digest;
                std::uint64_t blob_size{};
        };

        auto dir_exists(const fs::path&) -> bool;
        auto file_exists(const fs::path&) -> bool;
        auto ensure_dir(const fs::path&, mode_t mode = 0755) -> void;
        auto ensure_file(const fs::path&) -> void;
        auto write_file(const fs::path&, std::string_view, bool append_mode = false) -> void;
        auto copy_directory(const fs::path&, const fs::path&) -> void;
        auto remove_directory(const fs::path&) -> void;
        auto rename_file_or_directory(const fs::path&, const fs::path&) -> void;
        auto change_permissions(const fs::path&, mode_t) -> void;
        auto change_owners(const fs::path&, uid_t, gid_t) -> void;
        auto get_base_dir() -> fs::path;
        auto get_sock_path(std::string_view) -> fs::path;
        auto get_filesystem_path(std::string_view) -> fs::path;
        auto get_vfs_path(std::string_view) -> fs::path;
        auto get_layers_path(std::string_view) -> fs::path;
        auto get_image_path(std::string_view) -> fs::path;
        auto get_db_path(std::string_view) -> fs::path;
        auto get_log_path(std::string_view) -> fs::path;
        auto get_logger_command_queue_buf_name() -> std::string;
        auto get_database_command_queue_buf_name() -> std::string;
        auto get_value_heap_buf_name() -> std::string;
        auto get_device_gid(const fs::path&) -> gid_t;
        auto get_gid_map_payload(const std::vector<OCIRuntime::Device>&) -> std::string;
        auto find_program_path(const std::string&) -> fs::path;
        auto generate_container_id() -> std::string;
        auto spawn_new_consumer() -> pid_t;
        auto parse_subgid(const std::string&) -> std::vector<SubIDRange>;
        auto parse_subuid(const std::string&) -> std::vector<SubIDRange>;
        auto get_username() -> std::string;
        auto build_gid_map_payload(pid_t) -> std::string;
        auto write_all(int, const char*, ssize_t) -> bool;
        auto create_tar_gz(const fs::path&, const fs::path&) -> void;
        auto extract_tarball(const std::string&, const std::string&) -> void;
        auto is_archive(const fs::path&) -> bool;
        auto sha256(std::string_view) -> std::string;
        auto sha256_file(const fs::path&) -> std::string;
        auto print_usage() -> void;

        // OCI layer export utilities
        auto sha256_final(EVP_MD_CTX*) -> std::string;
        auto is_overlay_whiteout(const fs::path&) -> bool;
        auto is_opaque_directory(const fs::path&) -> bool;
        auto create_oci_layer(const fs::path& upper_dir, const fs::path& output_path) -> LayerInfo;
}
