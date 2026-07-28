#include "container_db_manager.hpp"
#include "container_metadata_generated.h"
#include "database_command_queue.hpp"
#include "serialization.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include <fcntl.h>
#include <iostream>
#include <format>
#include <cstring>
#include <flatbuffers/flatbuffer_builder.h>
#include <optional>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

auto ContainerDbManager::init() -> void {
        m_value_heap = &ValueHeap::get_instance();
        m_db_command_queue = &DatabaseCommandQueue::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                std::cerr << "Container Db";
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_db_command_queue->map_buffer(Utils::get_database_command_queue_buf_name(), false);
        if (!m_db_command_queue->ok()) [[unlikely]] {
                std::cerr << "Container Db\n";
                throw std::runtime_error(m_db_command_queue->get_error());
        }
        m_socket_path = "/tmp/quiver_db.sock";
        m_addr.sun_family = AF_UNIX;
        strncpy(m_addr.sun_path, m_socket_path.data(), sizeof(m_addr.sun_path)-1);
}

auto ContainerDbManager::add_container(const ContainerDbObject& db_object) -> void {
        flatbuffers::FlatBufferBuilder builder{};
        auto serialized_root{Serialization::serialize(builder, db_object)};
        builder.Finish(serialized_root);
        std::string value(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
        DatabaseJobData job_data{};
        job_data.target = TargetDB::CONTAINER;
        job_data.type = JobType::PUT;
        job_data.value_length = value.length();
        std::memcpy(job_data.key, db_object.config.container_id.data(), 32);
        while (!m_value_heap->write_job_data(value, job_data.value_offset)) {
                std::this_thread::yield();
        }
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
}

auto ContainerDbManager::remove_container(const std::string& key) -> void {
        auto metadata{get_container(key)};
        if (!metadata) {
                std::cerr << std::format("Error: Container '{}' not found.\n", key);
                return;
        }
        if (metadata->status == "running") {
                std::cerr << std::format("Error: Cannot remove container '{}' -> status is running\n", key);
                return;
        }
        DatabaseJobData job_data{};
        job_data.target = TargetDB::CONTAINER;
        job_data.type = JobType::DELETE;
        std::memcpy(job_data.key, key.data(), 32);
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
}

auto ContainerDbManager::update_container(const std::string& key, const ContainerDbObject& db_object) -> void {
        flatbuffers::FlatBufferBuilder builder{};
        auto serialized_root{Serialization::serialize(builder, db_object)};
        builder.Finish(serialized_root);
        std::string value(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
        DatabaseJobData job_data{};
        job_data.target = TargetDB::CONTAINER;
        job_data.type = JobType::UPDATE;
        job_data.value_length = value.length();
        std::memcpy(job_data.key, key.data(), 32);
        while (!m_value_heap->write_job_data(value, job_data.value_offset)) {
                std::this_thread::yield();
        }
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
}

auto ContainerDbManager::list_all_container() -> void {
        DatabaseJobData job_data{};
        job_data.target = TargetDB::CONTAINER;
        job_data.type = JobType::GETALL;
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }

        int connection_fd{socket(AF_UNIX, SOCK_STREAM, 0)};
        if (connection_fd == -1) {
                std::cerr << "Error: Unable to create socket: " << std::strerror(errno) << '\n';
                return;
        }
        while (connect(connection_fd, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr)) == -1) {
                if (errno != ENOENT && errno != ECONNREFUSED) {
                        std::cerr << "Error: Unable to connect: " << std::strerror(errno) << '\n';
                        close(connection_fd);
                        return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        constexpr int id_width{32};
        constexpr int image_width{20};
        constexpr int name_width{20};
        constexpr int status_width{10};
        std::cout << std::format( "{:<{}}  {:<{}}  {:<{}}  {:<{}}  {}\n", "CONTAINER ID", id_width, "IMAGE", image_width,
                        "NAME", name_width, "STATUS", status_width, "CREATED");

        size_t n_entries{0};
        if (!Utils::recv_all(connection_fd, &n_entries, sizeof(n_entries))) {
                std::cerr << "Error: Unable to get number of entries\n";
                return;
        }
        auto boot_time{Utils::get_boot_time()};
        for (size_t i{0}; i<n_entries; ++i) {
                size_t key_size{};
                if (!Utils::recv_all(connection_fd, &key_size, sizeof(key_size))) {
                        std::cerr << "Error reading key size.\n";
                        break;
                }
                std::string key(key_size, '\0');
                if (!Utils::recv_all(connection_fd, key.data(), key_size)) {
                        std::cerr << "Error reading key.\n";
                        break;
                }
                size_t value_size{};
                if (!Utils::recv_all(connection_fd, &value_size, sizeof(value_size))) {
                        std::cerr << "Error reading value size.\n";
                        break;
                }
                std::string value(value_size, '\0');
                if (!Utils::recv_all(connection_fd, value.data(), value_size)) {
                        std::cerr << "Error reading value.\n";
                        break;
                }
                auto metadata{extract_metadata(value)};
                if (!metadata) continue;
                if (metadata->status == "running" && !Utils::is_process_alive(metadata->config.pid, key)) {
                        if (metadata->boot_time < boot_time) {
                                metadata->status = "interrupted by reboot";
                        }
                        else {
                                metadata->status = "killed";
                        }
                        update_container(key, metadata.value());
                }
                std::cout << std::format("{:<{}}  {:<{}}  {:<{}}  {:<{}}  {}\n", key, id_width, metadata->image, image_width,
                                metadata->name, name_width, metadata->status, status_width, metadata->created_at);
        }
        close(connection_fd);
}

auto ContainerDbManager::list_all_running_container() -> void {
        DatabaseJobData job_data{};
        job_data.target = TargetDB::CONTAINER;
        job_data.type = JobType::GETALL;
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }

        int connection_fd{socket(AF_UNIX, SOCK_STREAM, 0)};
        if (connection_fd == -1) {
                std::cerr << "Error: Unable to create socket: " << std::strerror(errno) << '\n';
                return;
        }
        while (connect(connection_fd, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr)) == -1) {
                if (errno != ENOENT && errno != ECONNREFUSED) {
                        std::cerr << "Error: Unable to connect: " << std::strerror(errno) << '\n';
                        close(connection_fd);
                        return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        constexpr int id_width{32};
        constexpr int image_width{20};
        constexpr int name_width{20};
        constexpr int status_width{10};
        std::cout << std::format( "{:<{}}  {:<{}}  {:<{}}  {:<{}}  {}\n", "CONTAINER ID", id_width, "IMAGE", image_width,
                        "NAME", name_width, "STATUS", status_width, "CREATED");
        size_t n_entries{0};
        if (!Utils::recv_all(connection_fd, &n_entries, sizeof(n_entries))) {
                std::cerr << "Error: Unable to get number of entries\n";
                return;
        }

        auto boot_time{Utils::get_boot_time()};

        for (size_t i{0}; i<n_entries; ++i) {
                size_t key_size{};
                if (!Utils::recv_all(connection_fd, &key_size, sizeof(key_size))) {
                        std::cerr << "Error reading key size.\n";
                        break;
                }
                std::string key(key_size, '\0');
                if (!Utils::recv_all(connection_fd, key.data(), key_size)) {
                        std::cerr << "Error reading key.\n";
                        break;
                }
                size_t value_size{};
                if (!Utils::recv_all(connection_fd, &value_size, sizeof(value_size))) {
                        std::cerr << "Error reading value size.\n";
                        break;
                }
                std::string value(value_size, '\0');
                if (!Utils::recv_all(connection_fd, value.data(), value_size)) {
                        std::cerr << "Error reading value.\n";
                        break;
                }
                auto metadata{extract_metadata(value)};
                if (!metadata) continue;
                if (metadata->status == "running" && !Utils::is_process_alive(metadata->config.pid, key)) {
                        if (metadata->boot_time < boot_time) {
                                metadata->status = "interrupted by reboot";
                        }
                        else {
                                metadata->status = "killed";
                        }
                        update_container(key, metadata.value());
                }
                if (metadata->status != "running") continue;
                std::cout << std::format("{:<{}}  {:<{}}  {:<{}}  {:<{}}  {}\n", key, id_width, metadata->image, image_width,
                                metadata->name, name_width, metadata->status, status_width, metadata->created_at);
        }
        close(connection_fd);
}

auto ContainerDbManager::get_container(const std::string& key) -> std::optional<ContainerDbObject> {
        DatabaseJobData job_data{};
        job_data.target = TargetDB::CONTAINER;
        job_data.type = JobType::GET;
        std::memcpy(job_data.key, key.data(), 32);
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
        int connection_fd{socket(AF_UNIX, SOCK_STREAM, 0)};
        if (connection_fd == -1) [[unlikely]] {
                std::cerr << "Error: Unable to create socket: " << std::strerror(errno) << '\n';
                return std::nullopt;
        }
        while (connect(connection_fd, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr)) == -1) {
                if (errno != ENOENT && errno != ECONNREFUSED) {
                        std::cerr << "Error: Unable to connect: " << std::strerror(errno) << '\n';
                        close(connection_fd);
                        return std::nullopt;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        size_t result_size{};
        if (!Utils::recv_all(connection_fd, &result_size, sizeof(result_size))) [[unlikely]] {
                std::cerr << "Error: Unable to read result bytes\n";
                close(connection_fd);
                return std::nullopt;
        }

        if (result_size == 0) {
                close(connection_fd);
                return std::nullopt;
        }
        std::string raw_bytes{};
        raw_bytes.resize(result_size);
        if (!Utils::recv_all(connection_fd, &raw_bytes[0], result_size)) [[unlikely]] {
                std::cerr << "Error: Unable to read raw bytes\n";
                close(connection_fd);
                return std::nullopt;
        }
        close(connection_fd);
        auto metadata{extract_metadata(raw_bytes)};
        auto boot_time{Utils::get_boot_time()};
        if (!metadata) {
                std::cerr << std::format("Serialization Error: Unable to deserialize the value for key '{}'\n", key);
                return std::nullopt;
        }
        if (metadata->status == "running" && !Utils::is_process_alive(metadata->config.pid, key)) {
                if (metadata->boot_time < boot_time) {
                        metadata->status = "interrupted by reboot";
                }
                else {
                        metadata->status = "killed";
                }
                update_container(key, metadata.value());
        }
        return metadata;
}

auto ContainerDbManager::inspect_container(const std::string& key) -> void {
        auto metadata{get_container(key)};
        if (metadata) {
                if (!Utils::is_process_alive(metadata->config.pid, key)) {
                        metadata->status = "killed";
                        update_container(key, metadata.value());
                }
                PrintUtils::print_container_config(metadata->config);
        }
}

auto ContainerDbManager::extract_metadata(const std::string& raw_bytes) -> std::optional<ContainerDbObject> {
        flatbuffers::Verifier verifier{reinterpret_cast<const uint8_t*>(raw_bytes.data()), raw_bytes.size()};
        if (!verifier.VerifyBuffer<FB::ContainerMetadata>(nullptr)) {
                return std::nullopt;
        }
        const auto* fb_root{flatbuffers::GetRoot<FB::ContainerMetadata>(raw_bytes.data())};
        auto metadata{Serialization::deserialize(fb_root)};
        return metadata;
}
