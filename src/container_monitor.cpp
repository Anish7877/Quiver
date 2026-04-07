#include "container_monitor.hpp"
#include "container_runtime.hpp"
#include "pasta_network.hpp"
#include "pty_session_manager.hpp"
#include "types.hpp"
#include "value_heap.hpp"
#include "logger_command_queue.hpp"
#include "utils.hpp"
#include "cgroups_manager_interface.hpp"
#include "cgroups_manager_creator.hpp"
#include <chrono>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <format>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <poll.h>
namespace chrono = std::chrono;

auto ContainerMonitor::init(const ContainerConfig& config) -> void {
        m_container_config = config;
        m_value_heap = &ValueHeap::get_instance();
        m_log_cmd_queue = &LoggerCommandQueue::get_instance();
        m_pty_session_manager = &PtySessionManager::get_instance();
        m_cgroups_manager = CGroupsManagerCreator::create_cgourps_manager(m_container_config.container_id, m_container_config.cgroups_path);
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
        setup_set_groups();
        setup_uid_map();
        setup_gid_map();
}

auto ContainerMonitor::start_logging(int master_fd) -> bool {
        m_logging_active.store(true, std::memory_order_release);
        m_log_worker = std::thread([&]() {
                        if(m_container_config.terminal.value) {
                        pollfd fds[1];
                        fds[0].fd = master_fd;
                        fds[0].events = POLLIN;

                        while(m_logging_active.load(std::memory_order_acquire) && fds[0].fd != -1) {
                        int ret{poll(fds, 1, 100)};

                        if (ret < 0) {
                        if(errno == EINTR) continue;
                        break;
                        }
                        if (ret == 0) continue;

                        if (fds[0].fd != -1 && (fds[0].revents & POLLIN)) {
                        char buffer[4096];
                        ssize_t bytes_read{read(fds[0].fd, buffer, sizeof(buffer))};

                        if (bytes_read > 0) {
                        std::string log_data{std::format("[{}] [{}] [PTY] {}.",
                                        chrono::system_clock::now(),
                                        m_container_id,
                                        std::string(buffer, bytes_read))};

                        this->log_event(log_data, TargetLog::CONTAINERLOG);

                        }
                        }

                        if (fds[0].fd != -1 && (fds[0].revents & POLLHUP)) {
                                fds[0].fd = -1;
                        }
                        }

                        if (master_fd != -1) {
                                close(master_fd);
                        }
                        }
                        else {
                                pollfd fds[2];
                                fds[0].fd = m_std_out_fd[0];
                                fds[0].events = POLLIN;
                                fds[1].fd = m_std_err_fd[0];
                                fds[1].events = POLLIN;

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
                                        if (fds[0].fd != -1 && (fds[0].revents & POLLIN)) {
                                                char buffer[4096];
                                                ssize_t bytes_read{read(fds[0].fd, buffer, sizeof(buffer))};
                                                if (bytes_read > 0) {
                                                        std::string log_data{std::format("[{}] [{}] [STDOUT] {}.",
                                                                        chrono::system_clock::now(),
                                                                        m_container_id,
                                                                        std::string(buffer, bytes_read))};
                                                        this->log_event(log_data, TargetLog::CONTAINERLOG);
                                                }
                                        }
                                        if (fds[0].fd != -1 && (fds[0].revents & POLLHUP)) {
                                                fds[0].fd = -1;
                                                --open_pipes;
                                        }
                                        if (fds[1].fd != -1 && (fds[1].revents & POLLIN)) {
                                                char buffer[4096];
                                                ssize_t bytes_read{read(fds[1].fd, buffer, sizeof(buffer))};
                                                if (bytes_read > 1) {
                                                        std::string log_data{std::format("[{}] [{}] [STDERR] {}.",
                                                                        chrono::system_clock::now(),
                                                                        m_container_id,
                                                                        std::string(buffer, bytes_read))};
                                                        this->log_event(log_data, TargetLog::CONTAINERLOG);
                                                }
                                        }
                                        if (fds[1].fd != -1 && (fds[1].revents & POLLHUP)) {
                                                fds[1].fd = -1;
                                                --open_pipes;
                                        }
                                }

                                if (m_std_out_fd[0] != -1) close(m_std_out_fd[0]);
                                if (m_std_err_fd[0] != -1) close(m_std_err_fd[0]);
                        }
        });
        return true;
}

auto ContainerMonitor::invoke_container() -> void {
        m_monitor_pid = fork();

        if (m_monitor_pid == -1) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] [{}] Container Monitor Error: fork failed -> {}.",
                                        chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno)));
        }
        if(m_monitor_pid > 0) return;
        if (setsid() == -1) [[unlikely]] {
                std::string err{std::format("[{}] [{}] Container Monitor Fatal: setsid failed -> {}.",
                                chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno))};
                log_event(err, TargetLog::CONTAINERMON);
                return;
        }
        if (!attach_to_stdio()) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Monitor Fatal: Failed to attach stdio.",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERMON);
                return;
        }
        if (pipe(m_container_to_monitor_fd) == -1 || pipe(m_monitor_to_container_fd) == -1) [[unlikely]] {
                std::string err{std::format("[{}] [{}] Container Monitor Fatal: pipe creation failed.",
                                chrono::system_clock::now(), m_container_config.container_id)};
                log_event(err, TargetLog::CONTAINERMON);
                return;
        }
        m_container_pid = fork();
        if (m_container_pid == -1) [[unlikely]] {
                std::string err{std::format("[{}] [{}] Container Monitor Error: fork failed -> {}.",
                                chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno))};
                log_event(err, TargetLog::CONTAINERMON);
                return;
        }
        if (m_container_pid == 0) {
                close(m_container_to_monitor_fd[0]);
                close(m_monitor_to_container_fd[1]);

                int flags{};
                for (const auto& [path, namespace_str] : m_container_config.namespaces) {
                        auto it{OCIRuntime::NAMESPACE_STR_MAP.find(namespace_str)};
                        if (path.empty()) {
                                if (it != OCIRuntime::NAMESPACE_STR_MAP.end()) {
                                        flags |= it->second;
                                }
                                else [[unlikely]] {
                                        log_event(std::format("[{}] [{}] Container Runtime Error: Unknown or unsupported namespace requested -> {}.",
                                                                std::chrono::system_clock::now(), m_container_config.container_id,
                                                                namespace_str), TargetLog::CONTAINERLOG);
                                        _exit(EXIT_FAILURE);
                                }
                        }
                        else {
                                if (Utils::file_exists(path)) {
                                        int nstype{0};

                                        if (it != OCIRuntime::NAMESPACE_STR_MAP.end()) {
                                                nstype = it->second;
                                        }
                                        else [[unlikely]] {
                                                log_event(std::format("[{}] [{}] Container Runtime Error: Unknown or unsupported namespace requested -> {}.",
                                                                        std::chrono::system_clock::now(), m_container_config.container_id,
                                                                        namespace_str), TargetLog::CONTAINERLOG);
                                                _exit(EXIT_FAILURE);
                                        }

                                        int fd{open(path.c_str(), O_RDONLY | O_CLOEXEC)};
                                        if (fd == -1) [[unlikely]] {
                                                log_event(std::format("[{}] [{}] Container Runtime Error: Failed to open namespace path: {}.",
                                                                        std::chrono::system_clock::now(), m_container_config.container_id,
                                                                        path.string()), TargetLog::CONTAINERLOG);
                                                _exit(EXIT_FAILURE);
                                        }

                                        if (setns(fd, nstype) == -1) [[unlikely]] {
                                                log_event(std::format("[{}] [{}] Container Runtime Error: setns failed for path: {}. Errno: {}",
                                                                        std::chrono::system_clock::now(), m_container_config.container_id,
                                                                        path.string(), errno), TargetLog::CONTAINERLOG);
                                                close(fd);
                                                _exit(EXIT_FAILURE);
                                        }
                                        close(fd);
                                }
                                else [[unlikely]] {
                                        log_event(std::format("[{}] [{}] Container Runtime Error: Namespace path doesn't exist: {}.",
                                                                std::chrono::system_clock::now(), m_container_config.container_id,
                                                                path.string()), TargetLog::CONTAINERLOG);
                                        _exit(EXIT_FAILURE);
                                }
                        }
                }
                if (unshare(CLONE_NEWUSER | flags) == -1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: unshare(CLONE_NEWUSER) failed.",
                                                chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                        _exit(EXIT_FAILURE);
                }
                char ready{'1'};
                if (write(m_container_to_monitor_fd[1], &ready, 1) == -1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: write to container_to_monitor_fd failed.",
                                                chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                        close(m_container_to_monitor_fd[1]);
                        _exit(EXIT_FAILURE);
                }
                close(m_container_to_monitor_fd[1]);

                char go{};
                if (read(m_monitor_to_container_fd[0], &go, 1) != 1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: read failed from monitor_to_container_fd.",
                                                chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                        close(m_monitor_to_container_fd[0]);
                        _exit(EXIT_FAILURE);
                }
                close(m_monitor_to_container_fd[0]);
                if (m_container_config.terminal.value) {
                        close(m_control_sock[0]);

                        m_pty_session_manager->setup_pty();
                        if (!m_pty_session_manager->ok()) [[unlikely]] {
                                log_event(m_pty_session_manager->get_error(), TargetLog::CONTAINERMON);
                                _exit(EXIT_FAILURE);
                        }

                        m_pty_session_manager->send_master_fd(m_control_sock[1], m_pty_session_manager->get_master_fd());
                        if (!m_pty_session_manager->ok()) {
                                log_event(m_pty_session_manager->get_error(), TargetLog::CONTAINERMON);
                                _exit(EXIT_FAILURE);
                        }
                        setup_socket_connection();

                        close(m_control_sock[1]);
                        close(m_pty_session_manager->get_master_fd());
                }
                else {
                        close(m_std_out_fd[0]);
                        close(m_std_err_fd[0]);

                        dup2(m_std_out_fd[1], STDOUT_FILENO);
                        dup2(m_std_err_fd[1], STDERR_FILENO);

                        close(m_std_out_fd[1]);
                        close(m_std_err_fd[1]);
                }
                m_runtime = std::make_unique<ContainerRuntime>(m_container_config);
                m_runtime->run_container();
                _exit(EXIT_FAILURE);
        }
        else {
                close(m_monitor_to_container_fd[0]);
                close(m_container_to_monitor_fd[1]);

                try {
                        m_cgroups_manager->attach_process(m_container_pid);
                }
                catch (const std::exception& e) {
                        log_event(std::format("[{}] {}", chrono::system_clock::now(), e.what()), TargetLog::CONTAINERMON);
                }
                char buf{};
                if (read(m_container_to_monitor_fd[0], &buf, 1) != 1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Monitor Error: read from container_to_monitor_fd failed.",
                                                chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                        close(m_container_to_monitor_fd[0]);
                        return;
                }
                close(m_container_to_monitor_fd[0]);

                setup_usernamespace();
                PastaNetwork::setup_networking(m_container_pid, m_container_config.networks);

                bool network_ready{false};
                int max_retries{200};

                while (max_retries > 0) {
                        std::ifstream net_dev(std::format("/proc/{}/net/dev", m_container_pid));
                        if (net_dev.is_open()) {
                                std::string line;
                                std::getline(net_dev, line);
                                std::getline(net_dev, line);

                                while (std::getline(net_dev, line)) {
                                        if (line.find("lo:") == std::string::npos &&
                                            line.find("sit0:") == std::string::npos &&
                                            line.find("tunl0:") == std::string::npos &&
                                            line.find(":") != std::string::npos) {
                                                network_ready = true;
                                                break;
                                        }
                                }
                        }

                        if (network_ready) {
                                break;
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        max_retries--;
                }

                if (!network_ready) [[unlikely]] {
                        log_event(std::format("[{}] Container Monitor Fatal: pasta failed to configure network within 1 second.",
                                                m_container_config.container_id), TargetLog::CONTAINERLOG);
                        close(m_monitor_to_container_fd[1]);
                        return;
                }

                char go_sig{'1'};
                if (write(m_monitor_to_container_fd[1], &go_sig, 1) == -1) [[unlikely]] {
                        log_event(std::format("[{}] Container Monitor Error: write to monitor_to_container_fd failed.",
                                                m_container_config.container_id), TargetLog::CONTAINERLOG);
                        close(m_monitor_to_container_fd[1]);
                        return;
                }
                close(m_monitor_to_container_fd[1]);

                int master_fd{-1};
                if (m_container_config.terminal.value) {
                        close(m_control_sock[1]);

                        master_fd = m_pty_session_manager->recv_master_fd(m_control_sock[0]);
                        if (!m_pty_session_manager->ok()) [[unlikely]] {
                                log_event(m_pty_session_manager->get_error(), TargetLog::CONTAINERMON);
                                return;
                        }
                        close(m_control_sock[0]);
                }
                else {
                        close(m_std_out_fd[1]);
                        close(m_std_err_fd[1]);
                }
                start_logging(master_fd);
                int status{};
                if (waitpid(m_container_pid, &status, 0) == -1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Monitor Error: waitpid failed - '{}'.",
                                                chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno)),
                                        TargetLog::CONTAINERMON);
                        return;
                }
        }
}

auto ContainerMonitor::setup_set_groups() -> void {
        fs::path set_groups_path{std::format("/proc/{}/setgroups", m_container_pid)};
        std::string buf{"deny"};
        Utils::write_file(set_groups_path, buf);
}

auto ContainerMonitor::setup_uid_map() -> void {
        const char* newuidmap_path{"/usr/bin/newuidmap"};
        std::string payload{std::format("{} {} {}\n", m_container_config.uid_mapping.host_id,
                        m_container_config.uid_mapping.container_id, m_container_config.uid_mapping.size)};
        exec_mapping_tool(newuidmap_path, payload);
}

auto ContainerMonitor::setup_gid_map() -> void {
        const char* newgidmap_path{"/usr/bin/newgidmap"};
        std::string payload{std::format("{} {} {}\n", m_container_config.gid_mapping.host_id,
                        m_container_config.gid_mapping.container_id, m_container_config.gid_mapping.size)};
        payload += Utils::get_gid_map_payload(m_container_config.devices);
        exec_mapping_tool(newgidmap_path, payload);
}

auto ContainerMonitor::setup_socket_connection() -> void {
        m_sock_path = Utils::get_sock_path(m_container_config.container_id);
        unlink(m_sock_path.c_str());

        m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_socket_fd == -1) [[unlikely]] {
                log_event(std::format("[{}] Container Runtime Error: socket failed.", m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_sock_path.c_str(), sizeof(addr.sun_path)-1);

        if (bind(m_socket_fd, (sockaddr*)&addr, sizeof(addr)) == -1) [[unlikely]] {
                log_event(std::format("[{}] Container Runtime Error: bind failed.", m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }

        if (listen(m_socket_fd, 1) == -1) [[unlikely]] {
                log_event(std::format("[{}] Container Runtime Error: listen failed.", m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }
}

auto ContainerMonitor::attach_to_container(const std::string& container_id) -> void {
        m_sock_path = Utils::get_sock_path(container_id);

        m_connection_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_sock_path.c_str(), sizeof(addr.sun_path)-1);

        if (connect(m_connection_fd, (sockaddr*)&addr, sizeof(addr)) == -1) [[unlikely]] {
                log_event(std::format("[{}] Container Runtime Error: Connection to socket failed.", m_container_config.container_id), TargetLog::CONTAINERLOG);
                std::cerr << std::format("[{}] Container Runtime Error: Connection to socket failed.\n", m_container_config.container_id);
                _exit(EXIT_FAILURE);
        }

        m_pty_session_manager->enable_raw_mode();

        pollfd fds[2];
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = m_connection_fd;
        fds[1].events = POLLIN;

        bool saw_ctrlp{false};
        bool should_run{true};
        bool user_detached{false};

        while (should_run) {
                int ret{poll(fds, 2, -1)};

                if (ret < 0) {
                        if (errno == EINTR) continue;
                        break;
                }

                if ((fds[0].revents | fds[1].revents) & (POLLERR | POLLHUP | POLLNVAL)) {
                        break;
                }

                if (fds[0].revents & POLLIN) {
                        char buf[4096];
                        ssize_t n{read(fds[0].fd, buf, sizeof(buf))};
                        if (n <= 0) {
                                should_run = false;
                                break;
                        }

                        for (ssize_t i = 0; i < n; ++i) {
                                unsigned char c{static_cast<unsigned char>(buf[i])};

                                if (saw_ctrlp) {
                                        if (c == 0x11) {
                                                should_run = false;
                                                user_detached = true;
                                                break;
                                        }
                                        unsigned char seq[2] { 0x10, c };
                                        if (write(m_connection_fd, seq, 2) <= 0) {
                                                should_run = false;
                                                break;
                                        }
                                        saw_ctrlp = false;
                                } else if (c == 0x10) {
                                        saw_ctrlp = true;
                                } else {
                                        if (write(m_connection_fd, &c, 1) <= 0) {
                                                should_run = false;
                                                break;
                                        }
                                }
                        }
                }

                if (!should_run) break;

                if (fds[1].revents & POLLIN) {
                        char buf[4096];
                        ssize_t n {read(fds[1].fd, buf, sizeof(buf))};
                        if (n <= 0) {
                                should_run = false;
                                break;
                        }
                        if (write(STDOUT_FILENO, buf, n) <= 0) {
                                should_run = false;
                                break;
                        }
                }
        }
        m_pty_session_manager->disable_raw_mode();
        if (user_detached) {
                fprintf(stderr, "[Detached]\n");
        }
        else {
                fprintf(stderr, "[Session Terminated]\n");
        }

        close (m_connection_fd);
}

auto ContainerMonitor::exec_mapping_tool(const char* binary_path, const std::string& payload) -> void {
        pid_t pid{fork()};

        if (pid == -1) [[unlikely]] {
                throw std::runtime_error(std::format("Container Monitor Error: fork failed for {} -> {}.",
                                        binary_path, std::strerror(errno)));
        }
        else if (pid == 0) {
                std::vector<std::string> args{};
                args.emplace_back(binary_path);
                args.emplace_back(std::to_string(m_container_pid));
                std::stringstream ss{payload};
                std::string token{};
                while (ss >> token) {
                        args.emplace_back(token);
                }
                std::vector<char*> c_args{};
                for(auto& arg : args) {
                        c_args.emplace_back(arg.data());
                }
                c_args.emplace_back(nullptr);

                execv(binary_path, c_args.data());

                std::cerr << std::format("Container Monitor Fatal: execl failed for {} -> {}.\n", binary_path, std::strerror(errno));
                _exit(EXIT_FAILURE);
        }
        else {
                int status{};
                if (waitpid(pid, &status, 0) == -1) [[unlikely]] {
                        throw std::runtime_error(std::format("Container Monitor Error: waitpid failed for {} -> {}",
                                                binary_path, std::strerror(errno)));
                }
                if (WIFEXITED(status)) {
                        int exit_code{WEXITSTATUS(status)};
                        if (exit_code > 0) [[unlikely]] {
                                throw std::runtime_error(std::format("Container Monitor Error: {} failed with user exit code {}.",
                                                        binary_path, exit_code));
                        }
                }
                else if (WIFSIGNALED(status)) {
                        throw std::runtime_error(std::format("Container Monitor Error: {} was terminated by signal {}.",
                                                binary_path, WTERMSIG(status)));
                }
        }
}

auto ContainerMonitor::attach_to_stdio() -> bool {
        if (m_container_config.terminal.value) {
                if (socketpair(AF_UNIX, SOCK_STREAM, 0, m_control_sock) == -1) [[unlikely]] return false;
        }
        else {
                if (pipe(m_std_out_fd) == -1 || pipe(m_std_err_fd) == -1) [[unlikely]]  return false;
        }
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
        while (!m_value_heap->write_job_data(log_data, offset)) {
                std::this_thread::yield();
        }
        m_log_job_data.target_log = target_log;
        m_log_job_data.value_offset = offset;
        m_log_job_data.value_length = log_data.size();
        while(!m_log_cmd_queue->atomic_push(m_log_job_data)) {
                std::this_thread::yield();
        }
}

ContainerMonitor::~ContainerMonitor() {
        stop_logging();

        if (m_connection_fd != -1) {
                close(m_connection_fd);
        }
        if (m_socket_fd != -1) {
                close(m_socket_fd);
        }
        if (!m_sock_path.empty()) {
                unlink(m_sock_path.c_str());
        }
}
