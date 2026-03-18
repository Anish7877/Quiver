#include "container_monitor.hpp"
#include "types.hpp"
#include "value_heap.hpp"
#include "logger_command_queue.hpp"
#include "utils.hpp"
#include <chrono>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <format>
#include <cstdlib>
#include <rocksdb/comparator.h>
#include <stdexcept>
#include <unistd.h>
#include <poll.h>
namespace chrono = std::chrono;

auto ContainerMonitor::init(const ContainerConfig& config) -> void {
        m_container_config = config;
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_log_cmd_queue->map_buffer(Utils::get_logger_command_queue_buf_name(), false);
        if (!m_log_cmd_queue->ok()) [[unlikely]] {
                throw std::runtime_error(m_log_cmd_queue->get_error());
        }
}

auto ContainerMonitor::setup_usernamespace() -> void {
        setup_uid_map();
        setup_gid_map();
}

auto ContainerMonitor::start_logging() -> bool {
        if (!attach_to_stdio()) return false;
        m_logging_active.store(true, std::memory_order_release);
        m_log_worker = std::thread([this]() {
                                pollfd fds[2];
                                fds[0].fd = m_std_out_fd[0];
                                fds[0].events = POLL_IN;
                                fds[1].fd = m_std_err_fd[0];
                                fds[1].events = POLL_IN;

                                int open_pipes{2};
                                while(m_logging_active.load(std::memory_order_acquire) && open_pipes > 0) {
                                        int ret{poll(fds, 2, 100)};

                                        if (ret == -1) [[unlikely]] {
                                                if(errno == EINTR) continue;
                                                break;
                                        }
                                        if (ret == 0) {
                                                continue;
                                        }
                                        if (fds[0].fd != -1) {
                                                char buffer[4096];
                                                ssize_t bytes_read{read(fds[0].fd, buffer, sizeof(buffer))};
                                                if (bytes_read > 0) {
                                                        std::string log_data{std::format("[{}] [{}] [STDOUT] {}.",
                                                                        chrono::high_resolution_clock::now(),
                                                                        m_container_id,
                                                                        std::string(buffer, bytes_read))};
                                                        this->log_event(log_data, TargetLog::CONTAINERLOG);
                                                }
                                                if (fds[0].revents & POLL_HUP) {
                                                        fds[0].fd = -1;
                                                        --open_pipes;
                                                }
                                        }
                                        if (fds[1].fd != -1) {
                                                char buffer[4096];
                                                ssize_t bytes_read{read(fds[1].fd, buffer, sizeof(buffer))};
                                                if (bytes_read > 1) {
                                                        std::string log_data{std::format("[{}] [{}] [STDERR] {}.",
                                                                        chrono::high_resolution_clock::now(),
                                                                        m_container_id,
                                                                        std::string(buffer, bytes_read))};
                                                        this->log_event(log_data, TargetLog::CONTAINERLOG);
                                                }
                                                if (fds[1].revents & POLL_HUP) {
                                                        fds[1].fd = -1;
                                                        --open_pipes;
                                                }
                                        }
                                }
                        });
        return true;
}

auto ContainerMonitor::invoke_container() -> void {
        m_monitor_pid = fork();

        if (m_monitor_pid == -1) [[unlikely]] {
                throw std::runtime_error(std::format("Container Monitor Error: fork failed -> {}."
                                        , std::strerror(errno)));
        }
        if(m_monitor_pid > 0) return;
        if (setsid() == -1) [[unlikely]] {
                std::string err{std::format("[{}] Container Monitor Fatal: setsid failed -> {}.", m_container_id, std::strerror(errno))};
                this->log_event(err, TargetLog::CONTAINERMON);
                _exit(EXIT_FAILURE);
        }
        if (pipe(m_container_to_monitor_fd) == -1 || pipe(m_monitor_to_container_fd) == -1) [[unlikely]] {
                std::string err{"Container Monitor Fatal: pipe creation failed."};
                this->log_event(err, TargetLog::CONTAINERMON);
                _exit(EXIT_FAILURE);
        }
        m_container_pid = fork();
        if (m_container_pid == -1) [[unlikely]] {
                std::string err{std::format("Container Monitor Error: fork failed -> {}.", std::strerror(errno))};
                this->log_event(err, TargetLog::CONTAINERMON);
                _exit(EXIT_FAILURE);
        }
        if (m_container_pid == 0) {
                close(m_container_to_monitor_fd[0]);
                close(m_monitor_to_container_fd[1]);

                close(m_std_out_fd[0]);
                close(m_std_err_fd[0]);

                dup2(m_std_out_fd[1], STDOUT_FILENO);
                dup2(m_std_err_fd[1], STDERR_FILENO);

                close(m_std_out_fd[1]);
                close(m_std_err_fd[1]);
        }
        else {
                close(m_monitor_to_container_fd[0]);
                close(m_container_to_monitor_fd[1]);
                start_logging();
                close(m_std_out_fd[1]);
                close(m_std_err_fd[1]);
        }
}

auto ContainerMonitor::setup_uid_map() -> void {
        const char* newuidmap_path{"/usr/bin/newuidmap"};
        const char* newuidmap{"newuidmap"};
        exec_mapping_tool(newuidmap_path, newuidmap);
}

auto ContainerMonitor::setup_gid_map() -> void {
        const char* newgidmap_path{"/usr/bin/newgidmap"};
        const char* newgidmap{"newgidmap"};
        exec_mapping_tool(newgidmap_path, newgidmap);
}

auto ContainerMonitor::exec_mapping_tool(const char* binary_path, const char* binary_name) -> void {
        pid_t pid{fork()};

        if (pid == -1) [[unlikely]] {
                throw std::runtime_error(std::format("Container Monitor Error: fork failed for {} -> {}.",
                                                     binary_name, std::strerror(errno)));
        }
        else if (pid == 0) {
                execl(binary_path, binary_name, static_cast<char*>(NULL));
                std::cerr << std::format("Container Monitor Fatal: execl failed for {} -> {}.\n", binary_name, std::strerror(errno));
                _exit(EXIT_FAILURE);
        }
        else {
                int status{};
                if (waitpid(pid, &status, 0) == -1) [[unlikely]] {
                        throw std::runtime_error(std::format("Container Monitor Error: waitpid failed for {} -> {}",
                                                             binary_name, std::strerror(errno)));
                }
                if (WIFEXITED(status)) {
                        int exit_code{WEXITSTATUS(status)};
                        if (exit_code > 0) [[unlikely]] {
                                throw std::runtime_error(std::format("Container Monitor Error: {} failed with user exit code {}.",
                                                                     binary_name, exit_code));
                        }
                }
                else if (WIFSIGNALED(status)) {
                        throw std::runtime_error(std::format("Container Monitor Error: {} was terminated by signal {}.",
                                                             binary_name, WTERMSIG(status)));
                }
        }
}

auto ContainerMonitor::attach_to_stdio() -> bool {
        if (pipe(m_std_out_fd) == -1 || pipe(m_std_err_fd) == -1) return false;
        return true;
}

auto ContainerMonitor::stop_logging() -> void {
        if (m_logging_active.load(std::memory_order_acquire)) {
                m_logging_active.store(false, std::memory_order_release);
                if (m_log_worker.joinable()) {
                        m_log_worker.join();
                }
        }
}

auto ContainerMonitor::log_event(const std::string& log_data, TargetLog target_log) -> void {
                std::size_t offset{};
                while(!m_value_heap->write_job_data(log_data, offset)){}
                m_log_job_data.target_log = target_log;
                m_log_job_data.value_offset = offset;
                m_log_job_data.value_length = log_data.size();
                while(!m_log_cmd_queue->atomic_push(m_log_job_data)){}
}

ContainerMonitor::~ContainerMonitor() {
        stop_logging();
}
