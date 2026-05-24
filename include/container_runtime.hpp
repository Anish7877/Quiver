#pragma once
#include "container_config.hpp"
#include "types.hpp"
#include <memory>

class LoggerCommandQueue;
class ValueHeap;
class CapsManager;
class SeccompProfileManager;
class PtySessionManager;
class ContainerRuntime {
        public:
                explicit ContainerRuntime(const ContainerConfig&);
                ~ContainerRuntime();
                ContainerRuntime(const ContainerRuntime&) = delete;
                ContainerRuntime(ContainerRuntime&&) = delete;
                auto operator=(const ContainerRuntime&&) -> ContainerRuntime& = delete;
                auto operator=(ContainerRuntime&&) -> ContainerRuntime& = delete;

                auto run_container() -> void;
        private:
                auto exec_commands() -> void;
                auto execute_container_init() -> void;
                auto setup_root_filesystem() -> void;
                auto jail_process() -> void;
                auto mount_necessary_dirs() -> void;
                auto apply_rlimits() -> void;
                auto setup_standard_symlinks() -> void;
                auto setup_environment_variables() -> void;
                auto setup_security_paths() -> void;
                auto recv_slave_fd(int) -> void;
                auto supervise_container(pid_t) -> void;
                auto log_event(const std::string&) -> void;
                ContainerConfig m_container_config{};
                LogJobData m_log_job_data{};
                std::unique_ptr<CapsManager> m_caps_manager{};
                std::unique_ptr<SeccompProfileManager> m_seccomp_profile_manager{};
                LoggerCommandQueue* m_log_cmd_queue{};
                PtySessionManager* m_pty_session_manager{};
                ValueHeap* m_value_heap{};
};
