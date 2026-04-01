#include "log_job_processor.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "logger_command_queue.hpp"
#include "value_heap.hpp"
#include <atomic>
#include <chrono>
#include <exception>
#include <fstream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <thread>
namespace chrono = std::chrono;

auto LogJobProcessor::init() -> void {
        m_container_log_path = Utils::get_log_path("container");
        m_container_monitor_log_path = Utils::get_log_path("container_monitor");
        m_database_log_path = Utils::get_log_path("database");
        m_log_processor_log_path = Utils::get_log_path("log_processor");
        m_log_file.open(m_log_processor_log_path, std::ios::app);

        m_value_heap = &ValueHeap::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, true);
        if (!m_value_heap->ok()) {
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_log_command_queue->map_buffer(Utils::get_logger_command_queue_buf_name(), true);
        if (!m_log_command_queue->ok()) {
                throw std::runtime_error(m_log_command_queue->get_error());
        }
}

auto LogJobProcessor::process_job() -> void {
        m_running.store(true, std::memory_order_release);
        m_worker = std::thread([this]() {
                                while (this->m_running.load(std::memory_order_acquire)) {
                                        auto log_data{m_log_command_queue->atomic_pop()};
                                        if (log_data == std::nullopt) {
                                                std::this_thread::sleep_for(chrono::milliseconds(1));
                                                continue;
                                        }
                                        this->route_job(log_data.value());
                                }
                                while(true) {
                                        auto log_data{m_log_command_queue->atomic_pop()};
                                        if (log_data == std::nullopt) {
                                                break;
                                        }
                                        this->route_job(log_data.value());
                                }
                        });
}

auto LogJobProcessor::stop() -> void {
        m_running.store(false, std::memory_order_release);
        if (m_worker.joinable()) {
                m_worker.join();
        }
}

auto LogJobProcessor::route_job(const LogJobData& data) -> void {
        switch (data.target_log) {
                case TargetLog::DBLOG:
                        this->process_log(m_database_log_path, data);
                        break;
                case TargetLog::CONTAINERLOG:
                        this->process_log(m_container_log_path, data);
                        break;
                case TargetLog::CONTAINERMON:
                        this->process_log(m_container_monitor_log_path, data);
                        break;
                default:
                        m_log_file << std::format("[{}] Log Job Processor Error: Unknown job type found.\n",
                                        chrono::system_clock::now()) << std::flush;
                        break;
        }
}

auto LogJobProcessor::process_log(const fs::path& path, const LogJobData& job_data) -> void {
        const char* data_ptr{m_value_heap->get_job_data_pointer(job_data.value_offset)};
        if (data_ptr == nullptr) {
                m_log_file << std::format("[{}] Log Job Processor Error: null pointer exception for {} type job.\n",
                                chrono::system_clock::now(), static_cast<std::uint8_t>(job_data.target_log))
                           << std::flush;
                return;
        }
        std::string value(data_ptr, job_data.value_length);
        try {
                Utils::write_file(path, value, true);
                m_value_heap->commit_read_head(job_data.value_length);
        }
        catch (const std::exception& e) {
                m_log_file << std::format("[{}] Log Job Processor Error: got error -> '{}'.\n",
                                chrono::system_clock::now(), e.what())
                           << std::flush;
        }
}
