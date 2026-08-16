#include "image_db_manager.hpp"
#include "image_metadata_generated.h"
#include "value_heap.hpp"
#include "database_command_queue.hpp"
#include "serialization.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <flatbuffers/flatbuffer_builder.h>
#include <optional>
#include <thread>
#include <sys/socket.h>
#include <vector>

auto ImageDbManager::init() -> void {
        m_value_heap = &ValueHeap::get_instance();
        m_db_command_queue = &DatabaseCommandQueue::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_db_command_queue->map_buffer(Utils::get_database_command_queue_buf_name(), false);
        if (!m_db_command_queue->ok()) [[unlikely]] {
                throw std::runtime_error(m_db_command_queue->get_error());
        }
}

auto ImageDbManager::add_image(const ImageMetadata& image_data) -> void {
        flatbuffers::FlatBufferBuilder builder{};
        auto serialized_root{Serialization::serialize(builder, image_data)};
        builder.Finish(serialized_root);
        std::string value(reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize());
        DatabaseJobData job_data{};
        job_data.target = TargetDB::IMAGE;
        job_data.type = JobType::PUT;
        job_data.value_length = value.length();
        std::memcpy(job_data.key, image_data.id.data(), sizeof(job_data.key));
        while (!m_value_heap->write_job_data(value, job_data.value_offset)) {
                std::this_thread::yield();
        }
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
}

auto ImageDbManager::remove_image(const std::string& key) -> void {
        auto metadata{get_image(key)};
        if (!metadata) {
                std::cerr << std::format("Error: Image '{}' not found\n", key);
                return;
        }
        DatabaseJobData job_data{};
        job_data.target = TargetDB::IMAGE;
        job_data.type = JobType::DELETE;
        std::memcpy(job_data.key, key.data(), sizeof(job_data.key));
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
}

auto ImageDbManager::get_image(const std::string& key) -> std::optional<ImageMetadata> {
        std::string sock_path{std::format("/tmp/quiver_db_{}.sock", Utils::generate_id().substr(32))};
        int socket_fd{Utils::create_connection(sock_path)};
        DatabaseJobData job_data{};
        job_data.target = TargetDB::IMAGE;
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

        if (result_size == 0) [[unlikely]] {
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
        unlink(sock_path.c_str());
        auto metadata{extract_metadata(raw_bytes)};
        if (!metadata) {
                std::cerr << std::format("Serialization Error: Unable to deserialize the value for key '{}'\n", key);
                return std::nullopt;
        }
        return metadata;
}

auto ImageDbManager::get_all_images() -> std::vector<ImageMetadata> {
        std::string sock_path{std::format("/tmp/quiver_db_{}.sock", Utils::generate_id().substr(32))};
        int socket_fd{Utils::create_connection(sock_path)};
        DatabaseJobData job_data{};
        job_data.target = TargetDB::IMAGE;
        job_data.type = JobType::GETALL;
        std::memcpy(job_data.path, sock_path.c_str(), sock_path.length() + 1);
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
        std::vector<ImageMetadata> images{};
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
                        close(socket_fd);
                        unlink(sock_path.c_str());
                        std::cerr << "Error: Failed to connect to job processor\n";
                        return {};
                }
        }
        size_t n_entries{0};
        if (!Utils::recv_all(client_fd, &n_entries, sizeof(n_entries))) [[unlikely]] {
                unlink(sock_path.c_str());
                close(socket_fd);
                std::cerr << "Error: Unable to get number of entries\n";
                return images;
        }

        images.reserve(n_entries);
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
                images.emplace_back(metadata.value());
        }
        close(client_fd);
        close(socket_fd);
        unlink(sock_path.c_str());
        return images;
}

auto ImageDbManager::extract_metadata(const std::string& raw_bytes) -> std::optional<ImageMetadata> {
        flatbuffers::Verifier verifier{reinterpret_cast<const uint8_t*>(raw_bytes.data()), raw_bytes.size()};
        if (!verifier.VerifyBuffer<FB::ImageMetadata>(nullptr)) {
                return std::nullopt;
        }
        const auto* fb_root{flatbuffers::GetRoot<FB::ImageMetadata>(raw_bytes.data())};
        auto metadata{Serialization::deserialize(fb_root)};
        return metadata;
}
