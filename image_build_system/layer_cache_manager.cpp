#include "database_command_queue.hpp"
#include "layer_cache_manager.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include <flatbuffers/verifier.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <sys/socket.h>

[[nodiscard]] auto InFlightCacheManager::acquire(const std::string& hash) -> AcquireResult {
        std::shared_ptr<InFlightBuild> build{};
        if (m_inflight.find(hash, build)) {
                return {false, std::move(build)};
        }
        build = std::make_shared<InFlightBuild>();
        if (m_inflight.insert(hash, build)) {
                return {true, std::move(build)};
        }
        m_inflight.find(hash, build);
        return {false, std::move(build)};
}

auto InFlightCacheManager::finish_success(const std::string& hash, const std::pair<LayerCache, fs::path>& value) -> void {
        std::shared_ptr<InFlightBuild> build{};
        if (!m_inflight.find(hash, build)) {
                throw std::runtime_error("In Flight Cache Manager Error: Hash not found.");
        }
        build->promise.set_value(value);
        m_inflight.erase(hash);
}

auto InFlightCacheManager::finish_failure(const std::string& hash, std::exception_ptr eptr) -> void {
        std::shared_ptr<InFlightBuild> build{};
        if (!m_inflight.find(hash, build)) {
                throw std::runtime_error("In Flight Cache Manager Error: Hash not found.");
        }
        build->promise.set_exception(eptr);
        m_inflight.erase(hash);
}

auto LayerCacheManager::init() -> void {
        m_db_command_queue = &DatabaseCommandQueue::get_instance();
        m_value_heap = &ValueHeap::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_db_command_queue->map_buffer(Utils::get_database_command_queue_buf_name(), false);
        if (!m_db_command_queue->ok()) {
                throw std::runtime_error(m_db_command_queue->get_error());
        }
}

[[nodiscard]] auto LayerCacheManager::lookup(const std::string& key) -> std::optional<LayerCache> {
        std::string sock_path{std::format("/tmp/quiver_db_{}.sock", Utils::generate_container_id().substr(32))};
        int socket_fd{Utils::create_connection(sock_path)};
        DatabaseJobData job_data{};
        job_data.target = TargetDB::LAYERCACHE;
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
                throw("Error: Connection timeout reached\n");
        }
        else {
                client_fd = accept(socket_fd, nullptr, nullptr);
                if (client_fd == -1) [[unlikely]] {
                        throw("Error: Failed to connect to job processor\n");
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
        unlink(sock_path.c_str());
}

auto LayerCacheManager::store(const std::string& key, const LayerCache& cache) -> void {
}
