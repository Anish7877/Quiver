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
#include <sys/select.h>
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
                std::cerr << std::format("Error: Container '{}' not found\n", key);
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
        auto containers{get_all_container()};
        constexpr int id_width{70};
        constexpr int image_width{20};
        constexpr int name_width{20};
        constexpr int status_width{10};
        std::cout << std::format( "{:<{}}  {:<{}}  {:<{}}  {:<{}}  {}\n", "CONTAINER ID", id_width, "IMAGE", image_width,
                        "NAME", name_width, "STATUS", status_width, "CREATED");
        for (const auto& container : containers) {
                std::cout << std::format("{:<{}}  {:<{}}  {:<{}}  {:<{}}  {}\n", container.config.container_id, id_width, container.image, image_width,
                                container.name, name_width, container.status, status_width, container.created_at);
        }
}

auto ContainerDbManager::list_all_running_container() -> void {
        auto containers{get_all_container()};
        constexpr int id_width{70};
        constexpr int image_width{20};
        constexpr int name_width{20};
        constexpr int status_width{10};
        std::cout << std::format( "{:<{}}  {:<{}}  {:<{}}  {:<{}}  {}\n", "CONTAINER ID", id_width, "IMAGE", image_width,
                        "NAME", name_width, "STATUS", status_width, "CREATED");

        for (const auto& container : containers) {
                if (container.status != "running") continue;
                std::cout << std::format("{:<{}}  {:<{}}  {:<{}}  {:<{}}  {}\n", container.config.container_id, id_width, container.image, image_width,
                                container.name, name_width, container.status, status_width, container.created_at);
        }
}

auto ContainerDbManager::get_container(const std::string& key) -> std::optional<ContainerDbObject> {
        std::string sock_path{std::format("/tmp/quiver_db_{}.sock", Utils::generate_container_id().substr(32))};
        int socket_fd{Utils::create_connection(sock_path)};
        DatabaseJobData job_data{};
        job_data.target = TargetDB::CONTAINER;
        job_data.type = JobType::GET;
        std::memcpy(job_data.key, key.data(), sizeof(job_data.key));
        std::memcpy(job_data.path, sock_path.c_str(), sizeof(job_data.path));
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
        fd_set readfds{};
        timeval timeout{};
        int client_fd{-1};
        timeout.tv_usec = 0;
        timeout.tv_sec = 10;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        int result{select(socket_fd+1, &readfds, nullptr, nullptr, &timeout)};
        if (result < 0) [[unlikely]] {
                close(socket_fd);
                unlink(sock_path.c_str());
                return std::nullopt;
        }
        else if (result == 0) [[unlikely]] {
                close(socket_fd);
                unlink(sock_path.c_str());
                std::cerr << "Error: Connection timeout reached\n";
                return std::nullopt;
        }
        else {
                client_fd = accept(socket_fd, nullptr, nullptr);
                if (client_fd == -1) [[unlikely]] {
                        std::cerr << "Error: Failed to connect to job processor\n";
                        return std::nullopt;
                }
        }
        size_t result_size{};
        if (!Utils::recv_all(client_fd, &result_size, sizeof(result_size))) [[unlikely]] {
                std::cerr << "Error: Unable to read result bytes\n";
                close(client_fd);
                return std::nullopt;
        }

        if (result_size == 0) {
                close(client_fd);
                return std::nullopt;
        }
        std::string raw_bytes{};
        raw_bytes.resize(result_size);
        if (!Utils::recv_all(client_fd, &raw_bytes[0], result_size)) [[unlikely]] {
                std::cerr << "Error: Unable to read raw bytes\n";
                close(client_fd);
                return std::nullopt;
        }
        close(client_fd);
        close(socket_fd);
        auto metadata{extract_metadata(raw_bytes)};
        auto boot_time{Utils::get_boot_time()};
        if (!metadata) {
                std::cerr << std::format("Serialization Error: Unable to deserialize the value for key '{}'\n", key);
                return std::nullopt;
        }
        if (metadata->status == "running" && !Utils::is_process_alive(metadata->config.pid, metadata->config.container_id)) {
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

auto ContainerDbManager::get_all_container() -> std::vector<ContainerDbObject> {
        std::string sock_path{std::format("/tmp/quiver_db_{}.sock", Utils::generate_container_id().substr(32))};
        int socket_fd{Utils::create_connection(sock_path)};
        DatabaseJobData job_data{};
        job_data.target = TargetDB::CONTAINER;
        job_data.type = JobType::GETALL;
        std::memcpy(job_data.path, sock_path.c_str(), sock_path.length() + 1);
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
        std::vector<ContainerDbObject> containers{};
        fd_set readfds{};
        timeval timeout{};
        int client_fd{-1};
        timeout.tv_usec = 0;
        timeout.tv_sec = 10;
        FD_ZERO(&readfds);
        FD_SET(socket_fd, &readfds);
        int result{select(socket_fd+1, &readfds, nullptr, nullptr, &timeout)};
        if (result < 0) [[unlikely]] {
                close(socket_fd);
                unlink(sock_path.c_str());
                return {};
        }
        else if (result == 0) [[unlikely]] {
                close(socket_fd);
                unlink(sock_path.c_str());
                std::cerr << "Error: Connection timeout reached\n";
                return {};
        }
        else {
                client_fd = accept(socket_fd, nullptr, nullptr);
                if (client_fd == -1) [[unlikely]] {
                        std::cerr << "Error: Failed to connect to job processor\n";
                        return {};
                }
        }
        size_t n_entries{0};
        if (!Utils::recv_all(client_fd, &n_entries, sizeof(n_entries))) {
                std::cerr << "Error: Unable to get number of entries\n";
                return containers;
        }

        containers.reserve(n_entries);

        auto boot_time{Utils::get_boot_time()};

        for (size_t i{0}; i<n_entries; ++i) {
                size_t key_size{};
                if (!Utils::recv_all(client_fd, &key_size, sizeof(key_size))) {
                        std::cerr << "Error reading key size.\n";
                        break;
                }
                std::string key(key_size, '\0');
                if (!Utils::recv_all(client_fd, key.data(), key_size)) {
                        std::cerr << "Error reading key.\n";
                        break;
                }
                size_t value_size{};
                if (!Utils::recv_all(client_fd, &value_size, sizeof(value_size))) {
                        std::cerr << "Error reading value size.\n";
                        break;
                }
                std::string value(value_size, '\0');
                if (!Utils::recv_all(client_fd, value.data(), value_size)) {
                        std::cerr << "Error reading value.\n";
                        break;
                }
                auto metadata{extract_metadata(value)};
                if (!metadata) continue;
                if (metadata->status == "running" && !Utils::is_process_alive(metadata->config.pid, metadata->config.container_id)) {
                        if (metadata->boot_time < boot_time) {
                                metadata->status = "interrupted by reboot";
                        }
                        else {
                                metadata->status = "killed";
                        }
                        update_container(key, metadata.value());
                }
                containers.emplace_back(metadata.value());
        }
        close(client_fd);
        close(socket_fd);
        return containers;
}

auto ContainerDbManager::inspect_container(const std::string& key) -> void {
        auto metadata{get_container(key)};
        if (metadata) {
                if (!Utils::is_process_alive(metadata->config.pid, key)) {
                        metadata->status = "killed";
                        update_container(key, metadata.value());
                }
                PrintUtils::print_container_config(metadata->config);
                PrintUtils::print_field("Boot Time", metadata->boot_time);
        }
        else {
                std::cerr << std::format("Error: Container '{}' not found\n", key);
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
