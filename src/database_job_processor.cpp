#include "database_job_processor.hpp"
#include "database_command_queue.hpp"
#include "logger_command_queue.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/status.h>
#include <thread>
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
        job_data.status->m_ok = false;
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
        rocksdb::Slice key{job_data.key};
        std::string raw_bytes{};
        rocksdb::Status status{(*m_current_db)->Get(rocksdb::ReadOptions(), key, &raw_bytes)};
        if (status.IsNotFound()) [[unlikely]] {
                log_event(std::format("[{}] Database Job Processor Error: Key not found.",
                                        chrono::system_clock::now()));
                job_data.status->m_error = "Key Not Found.";
        }
        else if (!status.ok()) [[unlikely]] {
                log_event(std::format("[{}] Database Job Processor Error: Unable to process get job -> '{}'.",
                                        chrono::system_clock::now(), status.ToString()));
                job_data.status->m_error = status.ToString();
        }
        else {
                job_data.status->m_results.emplace_back(raw_bytes);
        }
        job_data.status->processed.store(true, std::memory_order_release);
        job_data.status->processed.notify_all();
}

auto DatabaseJobProcessor::process_put_job(const DatabaseJobData& job_data) -> void {
        const char* data_ptr{m_value_heap->get_job_data_pointer(job_data.value_offset)};
        if (data_ptr == nullptr) [[unlikely]] {
                log_event(std::format("[{}] Database Job Processor Error: Got nullptr in data pointer.",
                                        chrono::system_clock::now()));
                return;
        }
        rocksdb::Slice key{job_data.key};
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
        rocksdb::Slice key{job_data.key};
        rocksdb::Status status{(*m_current_db)->Delete(rocksdb::WriteOptions(), key)};
        if (!status.ok()) [[unlikely]] {
                log_event(std::format("[{}] Database Job Processor Error: Unable to process delete job -> '{}'.",
                                        chrono::system_clock::now(), status.ToString()));
        }
        else {
                job_data.status->m_ok = true;
        }
}

auto DatabaseJobProcessor::process_get_all_job(const DatabaseJobData& job_data) -> void {
        auto it{(*m_current_db)->NewIterator(rocksdb::ReadOptions())};
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
               job_data.status->m_results.emplace_back(std::move(it->key().ToString()), std::move(it->value().ToString()));
        }
        if (!it->status().ok()) [[unlikely]] {
               log_event(std::format("[{}] Database Job Processor Error: Read Error -> '{}'.",
                                        chrono::system_clock::now(), it->status().ToString()));
        }
        else {
               job_data.status->m_ok = true;
        }
        job_data.status->processed.store(true, std::memory_order_release);
        job_data.status->processed.notify_all();
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
