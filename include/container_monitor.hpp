#pragma once
#include "singleton.hpp"
#include "types.hpp"
#include <atomic>
#include <thread>

class LoggerCommandQueue;
class ValueHeap;
class ContainerMonitor : public Singleton<ContainerMonitor> {
        friend class Singleton<ContainerMonitor>;
        private:
                ContainerMonitor() = default;
                ~ContainerMonitor();
        public:
                ContainerMonitor(const ContainerMonitor&) = delete;
                ContainerMonitor(ContainerMonitor&&) = delete;
                auto operator=(const ContainerMonitor&) -> ContainerMonitor& = delete;
                auto operator=(ContainerMonitor&&) -> ContainerMonitor& = delete;

                auto init(const ContainerConfig&) -> void;
                auto setup_usernamespace() -> void;
                auto start_logging() -> bool;
                auto invoke_container() -> void;
        private:
                auto setup_uid_map() -> void;
                auto setup_gid_map() -> void;
                auto exec_mapping_tool(const char*, const char*) -> void;
                auto attach_to_stdio() -> bool;
                auto stop_logging() -> void;
                auto log_event(const std::string&, TargetLog) -> void;
                ContainerConfig m_container_config{};
                std::atomic<bool> m_logging_active{false};
                std::thread m_log_worker{};
                LogJobData m_log_job_data{};
                std::string m_container_id{};
                LoggerCommandQueue* m_log_cmd_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
                int m_std_out_fd[2]{-1, -1};
                int m_std_err_fd[2]{-1, -1};
                int m_container_to_monitor_fd[2]{-1, -1};
                int m_monitor_to_container_fd[2]{-1, -1};
                pid_t m_monitor_pid{-1};
                pid_t m_container_pid{-1};
};
