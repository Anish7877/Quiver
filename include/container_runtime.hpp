#pragma once
#include "container_config.hpp"
#include "types.hpp"
#include <memory>

class LoggerCommandQueue;
class ValueHeap;
class CapsManager;
class SeccompProfileManager;
class ContainerRuntime {
        public:
                explicit ContainerRuntime(const ContainerConfig&);
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
                auto setup_standard_symlinks() -> void;
                auto setup_environment_variables() -> void;
                auto setup_security_paths() -> void;
                auto supervise_container(pid_t) -> void;
                auto log_event(const std::string&) -> void;
                ContainerConfig m_container_config{};
                LogJobData m_log_job_data{};
                std::unique_ptr<CapsManager> m_caps_manager{};
                std::unique_ptr<SeccompProfileManager> m_seccomp_profile_manager{};
                LoggerCommandQueue* m_log_cmd_queue{};
                ValueHeap* m_value_heap{};
};
