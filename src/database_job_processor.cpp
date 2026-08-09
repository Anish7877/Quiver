#include "database_job_processor.hpp"
#include "database_command_queue.hpp"
#include "logger_command_queue.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include <asm-generic/errno.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <fcntl.h>
#include <sys/un.h>
#include <unistd.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/status.h>
#include <thread>
#include <sys/socket.h>
namespace chrono = std::chrono;

auto DatabaseJobProcessor::init() -> void {
        m_value_heap = &ValueHeap::get_instance();
        m_db_command_queue = &DatabaseCommandQueue::get_instance();
        m_log_command_queue = &LoggerCommandQueue::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, true);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_db_command_queue->map_buffer(Utils::get_database_command_queue_buf_name(), true);
        if (!m_db_command_queue->ok()) [[unlikely]] {
                throw std::runtime_error(m_db_command_queue->get_error());
        }
        m_log_command_queue->map_buffer(Utils::get_logger_command_queue_buf_name(), false);
        if (!m_log_command_queue->ok()) {
                throw std::runtime_error(m_log_command_queue->get_error());
        }
        m_current_db = nullptr;
        Utils::ensure_dir(Utils::get_db_path("container"));
        Utils::ensure_dir(Utils::get_db_path("image"));
        Utils::ensure_dir(Utils::get_db_path("layer_cache"));
        rocksdb::Options opts{};
        opts.create_if_missing = true;
        rocksdb::Status status{};
        status = rocksdb::DB::Open(opts, Utils::get_db_path("container"), &m_container_db);
        if (!status.ok()) [[unlikely]] {
                throw std::runtime_error(std::format("Database Job Error: Could not open container database -> '{}'.",
                                        status.ToString()));
        }
        status = rocksdb::DB::Open(opts, Utils::get_db_path("image"), &m_image_db);
        if (!status.ok()) [[unlikely]] {
                throw std::runtime_error(std::format("Database Job Error: Could not open image database -> '{}'.",
                                        status.ToString()));
        }
        status = rocksdb::DB::Open(opts, Utils::get_db_path("layer_cache"), &m_layer_cache_db);
        if (!status.ok()) [[unlikely]] {
                throw std::runtime_error(std::format("Database Job Error: Could not open layer cache database -> '{}'.",
                                        status.ToString()));
        }
}

auto DatabaseJobProcessor::process_job() -> void {
        m_running.store(true, std::memory_order_release);
        m_worker = std::jthread([this]() {
                                while (this->m_running.load(std::memory_order_acquire)) {
                                        auto job_data{this->m_db_command_queue->atomic_pop()};
                                        if (!job_data.has_value()) {
                                                std::this_thread::yield();
                                                continue;
                                        }
                                        route_job(job_data.value());
                                }
                                int retries_left{50};
                                while(retries_left > 0) {
                                        auto log_data{m_db_command_queue->atomic_pop()};
                                        if (log_data.has_value()) {
                                                this->route_job(log_data.value());
                                                retries_left = 50;
                                        }
                                        else {
                                                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                                                retries_left--;
                                        }
                                }
                        });
}

auto DatabaseJobProcessor::route_job(const DatabaseJobData& job_data) -> void {
        switch (job_data.target) {
                case TargetDB::CONTAINER:
                        m_current_db = &m_container_db; break;
                case TargetDB::IMAGE:
                        m_current_db = &m_image_db; break;
                case TargetDB::LAYERCACHE:
                        m_current_db = &m_layer_cache_db; break;
                default:
                        m_current_db = nullptr;
                        log_event(std::format("[{}] Database Job Processor Error: Unknown target DB found.",
                                                chrono::system_clock::now()));
        }
        if (m_current_db != nullptr) {
                switch (job_data.type) {
                        case JobType::GET:
                                process_get_job(job_data); break;
                        case JobType::PUT:
                                process_put_job(job_data); break;
                        case JobType::UPDATE:
                                process_update_job(job_data); break;
                        case JobType::DELETE:
                                process_delete_job(job_data); break;
                        case JobType::GETALL:
                                process_get_all_job(job_data); break;
                        default:
                                m_current_db = nullptr;
                                log_event(std::format("[{}] Database Job Processor Error: Unknown job type found.",
                                                        chrono::system_clock::now()));
                }
        }
}

auto DatabaseJobProcessor::process_get_job(const DatabaseJobData& job_data) -> void {
        rocksdb::Slice key{job_data.key, 32};
        std::string raw_bytes{};
        rocksdb::Status status{(*m_current_db)->Get(rocksdb::ReadOptions(), key, &raw_bytes)};

        int connection_fd{socket(AF_UNIX, SOCK_STREAM, 0)};
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, job_data.path, sizeof(addr.sun_path)-1);
        if (connection_fd == -1) [[unlikely]] {
                log_event(std::format("Database Job Processor Error: Unable to create socket: {}\n", std::strerror(errno)));
                return;
        }
        if (connect(connection_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
                if (errno == ENOENT || errno == ECONNREFUSED) {
                        log_event(std::format("[{}] Database Job Processor: Producer timed out. Dropping job.\n",
                                                chrono::system_clock::now()));
                }
                else {
                        log_event(std::format("[{}] Database Job Processor Error: Unable to connect: {}\n",
                                                chrono::system_clock::now(), std::strerror(errno)));
                }

                close(connection_fd);
        }

        if (status.IsNotFound() || !status.ok()) [[unlikely]] {
                if (status.IsNotFound()) {
                        log_event(std::format("[{}] Database Job Processor Error: Key not found\n", chrono::system_clock::now()));
                } else {
                        log_event(std::format("[{}] Database Job Processor Error: Get failed -> '{}'\n", chrono::system_clock::now(), status.ToString()));
                }

                size_t buf_size{0};
                send(connection_fd, &buf_size, sizeof(buf_size), 0);
        }
        else {
                size_t buf_size{raw_bytes.size()};
                if (send(connection_fd, &buf_size, sizeof(buf_size), 0) == -1) {
                        log_event(std::format("[{}] Database Job Processor Error: Failed to send buffer size.", chrono::system_clock::now()));
                        close(connection_fd);
                        return;
                }

                size_t total_sent{0};
                while (total_sent < buf_size) {
                        ssize_t n{send(connection_fd, raw_bytes.data() + total_sent, buf_size - total_sent, 0)};
                        if (n <= 0) break;
                        total_sent += n;
                }
        }
        close(connection_fd);
}

auto DatabaseJobProcessor::process_put_job(const DatabaseJobData& job_data) -> void {
        const char* data_ptr{m_value_heap->get_job_data_pointer(job_data.value_offset)};
        if (data_ptr == nullptr) [[unlikely]] {
                log_event(std::format("[{}] Database Job Processor Error: Got nullptr in data pointer.",
                                        chrono::system_clock::now()));
                return;
        }
        rocksdb::Slice key{job_data.key, 32};
        std::string value(data_ptr, job_data.value_length);
        rocksdb::Status status{(*m_current_db)->Put(rocksdb::WriteOptions(), key, value)};
        if (!status.ok()) [[unlikely]] {
                log_event(std::format("[{}] Database Job Processor Error: Unable to process put job -> '{}'.",
                                        chrono::system_clock::now(), status.ToString()));
        }
        else {
                m_value_heap->commit_read_head(job_data.value_length);
        }
}

auto DatabaseJobProcessor::process_update_job(const DatabaseJobData& job_data) -> void {
        process_put_job(job_data);
}

auto DatabaseJobProcessor::process_delete_job(const DatabaseJobData& job_data) -> void {
        rocksdb::Slice key{job_data.key, 32};
        rocksdb::Status status{(*m_current_db)->Delete(rocksdb::WriteOptions(), key)};
        if (!status.ok()) [[unlikely]] {
                log_event(std::format("[{}] Database Job Processor Error: Unable to process delete job -> '{}'.",
                                        chrono::system_clock::now(), status.ToString()));
        }
}

auto DatabaseJobProcessor::process_get_all_job(const DatabaseJobData& job_data) -> void {
        auto it1{(*m_current_db)->NewIterator(rocksdb::ReadOptions())};
        size_t count{0};
        for (it1->SeekToFirst(); it1->Valid(); it1->Next()) {
                ++count;
        }
        delete it1;
        int connection_fd{socket(AF_UNIX, SOCK_STREAM, 0)};
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, job_data.path, sizeof(addr.sun_path)-1);
        if (connection_fd == -1) [[unlikely]] {
                log_event(std::format("[{}] Database Job Processor Error: Unable to create socket: {}\n",
                                        chrono::system_clock::now(), std::strerror(errno)));
                return;
        }
        if (connect(connection_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
                if (errno == ENOENT || errno == ECONNREFUSED) {
                        log_event(std::format("[{}] Database Job Processor: Producer timed out. Dropping job.\n",
                                                chrono::system_clock::now()));
                }
                else {
                        log_event(std::format("[{}] Database Job Processor Error: Unable to connect: {}\n",
                                                chrono::system_clock::now(), std::strerror(errno)));
                }

                close(connection_fd);
        }
        if (!Utils::send_all(connection_fd, &count, sizeof(count))) [[unlikely]] {
                log_event(std::format("[{}] Database Job Error: Unable to send number of entries", chrono::system_clock::now()));
                return;
        }
        auto it2{(*m_current_db)->NewIterator(rocksdb::ReadOptions())};
        for (it2->SeekToFirst(); it2->Valid(); it2->Next()) {
                size_t key_size{it2->key().size()};
                size_t value_size{it2->value().size()};
                if (!Utils::send_all(connection_fd, &key_size, sizeof(key_size)))
                        return;

                if (!Utils::send_all(connection_fd, it2->key().data(), key_size))
                        return;

                if (!Utils::send_all(connection_fd, &value_size, sizeof(value_size)))
                        return;

                if (!Utils::send_all(connection_fd, it2->value().data(), value_size))
                        return;
        }
        if (!it2->status().ok()) [[unlikely]] {
                close(connection_fd);
                log_event(std::format("[{}] Database Job Processor Error: Read Error -> '{}'.",
                                        chrono::system_clock::now(), it2->status().ToString()));
        }
        delete it2;
}

auto DatabaseJobProcessor::log_event(const std::string& log_data) -> void {
        std::size_t offset{};
        while (!m_value_heap->write_job_data(log_data, offset)) {
                std::this_thread::yield();
        }
        m_log_data.target_log = TargetLog::DBLOG;
        m_log_data.value_offset = offset;
        m_log_data.value_length = log_data.length();
        while(!m_log_command_queue->atomic_push(m_log_data)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
}
