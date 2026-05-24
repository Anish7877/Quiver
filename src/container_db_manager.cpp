#include "container_db_manager.hpp"
#include "container_type_generated.h"
#include "logger_command_queue.hpp"
#include "value_heap.hpp"
#include "types.hpp"
#include "serialization.hpp"
#include "utils.hpp"
#include <chrono>
#include <flatbuffers/buffer.h>
#include <flatbuffers/verifier.h>
#include <format>
#include <thread>
namespace chrono = std::chrono;

auto ContainerDbManager::init() -> void {
        m_value_heap = &ValueHeap::get_instance();
        m_log_cmd_queue = &LoggerCommandQueue::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_log_cmd_queue->map_buffer(Utils::get_logger_command_queue_buf_name(), false);
        if (!m_log_cmd_queue->ok()) [[unlikely]] {
                throw std::runtime_error(m_log_cmd_queue->get_error());
        }
        rocksdb::Options options{};
        options.create_if_missing = true;
        rocksdb::Status status{rocksdb::DB::Open(options, m_db_path, &m_db)};
        if (!status.ok()) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] Container DB Manager Error: could not open database.",
                                        chrono::system_clock::now()));
        }
}

auto ContainerDbManager::process_job(const DatabaseJobData& job, const ContainerType& obj, Status& stat) -> void {
        stat.m_ok = false;
        switch (job.type) {
                case JobType::GET: process_get_job(job, stat); break;
                case JobType::PUT: process_put_job(job, obj, stat); break;
                case JobType::UPDATE: process_update_job(job, obj, stat); break;
                case JobType::DELETE: process_delete_job(job, stat); break;
                default:
                        log_event(std::format("[{}] Container DB Manager Error: Unknown job type found.",
                                        chrono::system_clock::now()));
                        stat.m_error = "Container DB Manager Error: Unknown job type found.";
        }
}

auto ContainerDbManager::extract_container(const std::string& raw_data, Status& stat) -> ContainerType {
        stat.m_ok = false;
        ContainerType obj{};
        flatbuffers::Verifier verifier{reinterpret_cast<const uint8_t*>(raw_data.data()), raw_data.size()};

        if (!verifier.VerifyBuffer<Types::Container>(nullptr)) {
                log_event(std::format("[{}] Container DB Manager Error: Data is corrupted or invalid flatbuffer data.",
                                chrono::system_clock::now()));
                stat.m_error = "Container DB Manager Error: Data is corrupted or invalid flatbuffer data.";
                return obj;
        }

        const auto* fb_root{flatbuffers::GetRoot<Types::Container>(raw_data.data())};
        obj = Serialization::deserialize(fb_root);
        stat.m_ok = true;
        return obj;
}

auto ContainerDbManager::process_get_job(const DatabaseJobData& job, Status& stat) -> void {
        if (m_db == nullptr) [[unlikely]] {
                log_event(std::format("[{}] Container DB Manager Error: Manager not initialized.",
                                chrono::system_clock::now()));
                stat.m_error = "Container DB Manager Error: Manager not initialized.";
                return;
        }
        rocksdb::Slice db_key{job.key};
        std::string fetched_raw_bytes{};
        rocksdb::Status status{m_db->Get(rocksdb::ReadOptions(), db_key, &fetched_raw_bytes)};

        if (status.IsNotFound()) [[unlikely]] {
                log_event(std::format("[{}] Container DB Manager Error: Key [{}] not found in database.",
                                chrono::system_clock::now(), job.key));
                stat.m_error = std::format("Container DB Manager Error: Key [{}] not found in database.", job.key);
        }
        else if (!status.ok()) [[unlikely]] {
                log_event(std::format("[{}] Container DB Manager Error: Read error -> {}.",
                                chrono::system_clock::now(), status.ToString()));
                stat.m_error = std::format("Container DB Manager Error: Read error -> {}.", status.ToString());
        }
        else {
                log_event(std::format("[{}] Container DB Manager: Get job success.", chrono::system_clock::now()));
                stat.m_ok = true;
                stat.m_result = std::move(fetched_raw_bytes);
        }
}

auto ContainerDbManager::process_put_job(const DatabaseJobData& job, const ContainerType& obj, Status& stat) -> void {
        if (m_db == nullptr) [[unlikely]] {
                log_event(std::format("[{}] Container DB Manager Error: Manager not initialized.",
                                        chrono::system_clock::now()));
                stat.m_error = "Container DB Manager Error: Manager not initialized.";
                return;
        }

        rocksdb::Slice db_key{job.key};
        flatbuffers::FlatBufferBuilder builder{};
        flatbuffers::Offset<Types::Container> offset{Serialization::serialize(builder, obj)};
        builder.Finish(offset);
        std::string serialized_value{reinterpret_cast<const char*>(builder.GetBufferPointer()), builder.GetSize()};
        rocksdb::Status status{m_db->Put(rocksdb::WriteOptions(), db_key, serialized_value)};

        if (!status.ok()) [[unlikely]] {
                log_event(std::format("[{}] Container DB Manager Error: Write error -> {}.",
                                        chrono::system_clock::now(), status.ToString()));
                stat.m_error = std::format("Container DB Manager Error: Write error -> {}.", status.ToString());
        }
        else {
                log_event(std::format("[{}] Container DB Manager: Put or Update job success.",
                                        chrono::system_clock::now()));
                stat.m_ok = true;
                stat.m_result = "Container DB Manager: Put or Update job success.";
        }
}

auto ContainerDbManager::process_update_job(const DatabaseJobData& job, const ContainerType& obj, Status& stat) -> void {
        process_put_job(job, obj, stat);
}

auto ContainerDbManager::process_delete_job(const DatabaseJobData& job, Status& stat) -> void {
        if (m_db == nullptr) [[unlikely]] {
                log_event(std::format("[{}] Container DB Manager Error: Manager not initialized.",
                                        chrono::system_clock::now()));
                stat.m_error = "Container DB Manager Error: Manager not initialized.";
                return;
        }

        rocksdb::Slice db_key{job.key};
        rocksdb::Status status{m_db->Delete(rocksdb::WriteOptions(), db_key)};

        if (!status.ok()) [[unlikely]] {
                log_event(std::format("[{}] Container DB Manager Error: Delete error -> {}.",
                                        chrono::system_clock::now(), status.ToString()));
                stat.m_error = std::format("[{}] Container DB Manager Error: Delete error -> {}.",
                                chrono::system_clock::now(), status.ToString());
        }
        else {
                log_event(std::format("[{}] Container DB Manager: Delete job success.",
                                        chrono::system_clock::now()));
                stat.m_ok = true;
                stat.m_result = "Container DB Manager: Delete job success.";
        }
}

auto ContainerDbManager::log_event(const std::string& log_data) -> void {
        std::size_t offset{};
        while (!m_value_heap->write_job_data(log_data, offset)) {
                std::this_thread::yield();
        };
        m_log_job_data.target_log = TargetLog::DBLOG;
        m_log_job_data.value_offset = offset;
        m_log_job_data.value_length = log_data.size();
        while (!m_log_cmd_queue->atomic_push(m_log_job_data)) {
                std::this_thread::yield();
        }
}

ContainerDbManager::~ContainerDbManager() {
        if (m_db != nullptr) {
                delete m_db;
        }
}
