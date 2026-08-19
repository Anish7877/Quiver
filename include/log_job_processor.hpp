#pragma once
#include "types.hpp"
#include "job_processor.hpp"
#include "singleton.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
namespace fs = std::filesystem;

class LoggerCommandQueue;
class ValueHeap;
class LogJobProcessor : public JobProcessor<LogJobData> , public Singleton<LogJobProcessor> {
        friend class Singleton<LogJobProcessor>;
        private:
                LogJobProcessor() = default;
                ~LogJobProcessor() = default;
        public:
                LogJobProcessor(const LogJobProcessor&) = delete;
                LogJobProcessor(LogJobProcessor&&) = delete;
                auto operator=(const LogJobProcessor&) -> LogJobProcessor& = delete;
                auto operator=(LogJobProcessor&&) -> LogJobProcessor& = delete;

                auto init() -> void override;
                auto process_job() -> void override;
                auto stop() -> void;
        private:
                auto route_job(const LogJobData&) -> void override;
                auto process_log(const fs::path&, const LogJobData&) -> void;
                fs::path m_container_log_path{};
                fs::path m_container_monitor_log_path{};
                fs::path m_database_log_path{};
                fs::path m_log_processor_log_path{};
                std::atomic<bool> m_running{};
                std::jthread m_worker{};
                std::ofstream m_log_file{};
                LoggerCommandQueue* m_log_command_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
};
