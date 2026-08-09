#pragma once
#include "container_config.hpp"
#include "oci_runtime.hpp"
#include "types.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <iostream>
#include <format>
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
        [[nodiscard]] auto get_vfs_path(std::string_view container_id) -> fs::path;
        [[nodiscard]] auto get_layers_path(std::string_view layer_name) -> fs::path;
        [[nodiscard]] auto sanitize_image_name(std::string_view image_name) -> std::string;
        [[nodiscard]] auto get_image_path(std::string_view image_name) -> fs::path;
        [[nodiscard]] auto get_db_path(std::string_view db_name) -> fs::path;
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
        auto resolve_user_group(const std::vector<std::string>&, const std::string&) -> std::pair<uid_t, gid_t>;
        auto build_gid_map_payload(pid_t) -> std::string;
        auto write_all(int, const char*, size_t) -> bool;
        auto create_tar_gz(const std::string&, const std::string&) -> void;
        auto extract_tarball(const std::string&, const std::string&) -> void;
        auto extract_oci_layer(const std::string&, const std::string&) -> void;
        auto is_archive(const fs::path&) -> bool;
        auto sha256(std::string_view) -> std::string;
        auto sha256_file(const fs::path&) -> std::string;
        auto print_usage() -> void;

        auto sha256_final(EVP_MD_CTX*) -> std::string;
        auto is_overlay_whiteout(const fs::path&) -> bool;
        auto is_opaque_directory(const fs::path&) -> bool;
        auto create_oci_layer(const fs::path& upper_dir, const fs::path& output_path) -> LayerInfo;

        [[nodiscard]] auto load_seccomp_profile(const fs::path&) -> OCIRuntime::Seccomp;
        auto send_all(int fd, const void* data, size_t size) -> bool;
        auto recv_all(int fd, void* data, size_t size) -> bool;
        [[nodiscard]] auto create_connection(std::string_view) -> int;

        auto is_process_alive(pid_t, const std::string&) -> bool;
        [[nodiscard]] auto get_boot_time() -> long;
}

namespace PrintUtils {
        constexpr int KEY_WIDTH{24};

        template<typename T>
        auto print_field(std::string_view key, const T& value) -> void {
                std::cout << std::format("{:<{}} : {}\n", key, KEY_WIDTH, value);
        }

        inline auto print_field(std::string_view key, bool value) -> void {
                std::cout << std::format("{:<{}} : {}\n",
                                key,
                                KEY_WIDTH,
                                value ? "true" : "false");
        }

        template<typename T>
        auto print_vector(std::string_view key, const std::vector<T>& values) -> void {
                if (values.empty())
                        return;
                std::cout << std::format("{:<{}} :\n", key, KEY_WIDTH);

                for (const auto& value : values)
                        std::cout << std::format("  • {}\n", value);
        }

        auto print_section(std::string_view name) -> void;
        auto print_container_config(const ContainerConfig&) -> void;
}
