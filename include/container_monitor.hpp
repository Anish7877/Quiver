#pragma once
#include "singleton.hpp"
#include "types.hpp"
#include "container_config.hpp"
#include <atomic>
#include <memory>
#include <thread>
#define CTRL_P 0x10
#define CTRL_Q 0x11

class LoggerCommandQueue;
class ValueHeap;
class PtySessionManager;
class ContainerRuntime;
class CGroupsManagerInterface;
class ContainerDbManager;
class ContainerMonitor : public Singleton<ContainerMonitor> {
        friend class Singleton<ContainerMonitor>;
        private:
                ContainerMonitor();
                ~ContainerMonitor();
        public:
                struct Limits {
                        int cpu_quota{};
                        std::uint64_t cpu_period{100000};
                        std::uint64_t cpu_weight{};
                        std::uint64_t memory_max{};
                        std::uint64_t memory_swap{};
                        std::uint64_t pids_limit{};
                        std::string cpuset_cpus{};
                        std::string cpuset_mems{};
                        std::vector<IOMaxUpdate> io_max_updates{};
                        std::vector<IOWeightUpdate> io_weight_updates{};
                };
                ContainerMonitor(const ContainerMonitor&) = delete;
                ContainerMonitor(ContainerMonitor&&) = delete;
                auto operator=(const ContainerMonitor&) -> ContainerMonitor& = delete;
                auto operator=(ContainerMonitor&&) -> ContainerMonitor& = delete;

                auto init(const ContainerConfig&, const std::string&, const std::string&, const Limits&, bool) -> void;
                auto setup_usernamespace() -> void;
                auto invoke_container() -> void;
                auto start_logging() -> void;
                auto foreground_logging() -> void;
                auto attach_to_container(const std::string&) -> void;
        private:
                auto run_container_child() -> void;
                auto run_monitor_parent() -> void;
                auto resolve_namespaces() -> int;
                auto wait_for_network() -> bool;
                auto setup_uid_map() -> void;
                auto setup_gid_map() -> void;
                auto setup_socket_connection() -> void;
                auto exec_mapping_tool(const char*, const std::string&) -> void;
                auto attach_to_stdio() -> bool;
                auto stop_logging() -> void;
                auto log_event(const std::string&, TargetLog) -> void;

                ContainerConfig m_container_config{};
                std::atomic<bool> m_logging_active{false};
                std::jthread m_log_worker{};
                LogJobData m_log_job_data{};
                std::string m_sock_path{};
                std::unique_ptr<ContainerRuntime> m_runtime{nullptr};
                std::unique_ptr<CGroupsManagerInterface> m_cgroups_manager{nullptr};
                LoggerCommandQueue* m_log_cmd_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
                PtySessionManager* m_pty_session_manager{nullptr};
                ContainerDbManager* m_container_db_manager{nullptr};
                Limits m_limits{};
                pid_t m_monitor_pid{-1};
                pid_t m_container_pid{-1};
                int m_cli_sync_pipe[2]{-1, -1};
                int m_std_out_fd[2]{-1, -1};
                int m_std_err_fd[2]{-1, -1};
                int m_container_to_monitor_fd[2]{-1, -1};
                int m_monitor_to_container_fd[2]{-1, -1};
                int m_control_sock[2]{-1, -1};
                int m_socket_fd{-1};
                int m_connection_fd{-1};
                bool destroy{false};
};
