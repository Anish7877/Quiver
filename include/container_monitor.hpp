#pragma once
#include "singleton.hpp"
#include "types.hpp"
#include <atomic>
#include <memory>
#include <thread>

class LoggerCommandQueue;
class ValueHeap;
class PtySessionManager;
class ContainerRuntime;
class CGroupsManager;
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
                auto invoke_container() -> void;
                auto start_logging(int master_fd = -1) -> bool;
        private:
                auto setup_set_groups() -> void;
                auto setup_uid_map() -> void;
                auto setup_gid_map() -> void;
                auto setup_socket_connection() -> void;
                auto attach_to_container(const std::string&) -> void;
                auto exec_mapping_tool(const char*, const std::string&) -> void;
                auto attach_to_stdio() -> bool;
                auto stop_logging() -> void;
                auto log_event(const std::string&, TargetLog) -> void;

                ContainerConfig m_container_config{};
                std::atomic<bool> m_logging_active{false};
                std::thread m_log_worker{};
                LogJobData m_log_job_data{};
                std::string m_container_id{};
                std::string m_sock_path{};
                std::unique_ptr<ContainerRuntime> m_runtime{nullptr};
                std::unique_ptr<CGroupsManager> m_cgroups_manager{nullptr};
                LoggerCommandQueue* m_log_cmd_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
                PtySessionManager* m_pty_session_manager{nullptr};
                int m_std_out_fd[2]{-1, -1};
                int m_std_err_fd[2]{-1, -1};
                int m_container_to_monitor_fd[2]{-1, -1};
                int m_monitor_to_container_fd[2]{-1, -1};
                int m_control_sock[2]{-1, -1};
                pid_t m_monitor_pid{-1};
                pid_t m_container_pid{-1};
                int m_socket_fd{-1};
                int m_connection_fd{-1};
};
