#pragma once
#include "types.hpp"

class LoggerCommandQueue;
class ValueHeap;
class PtySessionManager;
class ContainerRuntime {
        public:
                explicit ContainerRuntime(const ContainerConfig&, int, int);
                ~ContainerRuntime() = default;
                ContainerRuntime(const ContainerRuntime&) = delete;
                ContainerRuntime(ContainerRuntime&&) = delete;
                auto operator=(const ContainerRuntime&&) -> ContainerRuntime& = delete;
                auto operator=(ContainerRuntime&&) -> ContainerRuntime& = delete;

                auto exec_commands() -> void;
                auto pause_container() -> void;
                auto unpause_container() -> void;
                auto restart_container() -> void;
                auto run_container() -> void;
        private:
                auto execute_container_init() -> void;
                auto setup_root_filesystem() -> void;
                auto jail_process() -> void;
                auto mount_necessary_dirs() -> void;
                auto supervise_container(pid_t) -> void;
                auto log_event(const std::string&) -> void;
                ContainerConfig m_container_config{};
                LogJobData m_log_job_data{};
                LoggerCommandQueue* m_log_cmd_queue{};
                ValueHeap* m_value_heap{};
                PtySessionManager* m_pty_session_manager{};
                int m_container_to_monitor_fd{};
                int m_monitor_to_container_fd{};
};
