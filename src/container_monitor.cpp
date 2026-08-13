#include "cgroups_manager_creator.hpp"
#include "cgroups_manager_interface.hpp"
#include "container_monitor.hpp"
#include "container_db_manager.hpp"
#include "container_runtime.hpp"
#include "logger_command_queue.hpp"
#include "pasta_network.hpp"
#include "pty_session_manager.hpp"
#include "scoped_guard.hpp"
#include "signal_handler.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include "container_db_manager.hpp"
#include <atomic>
#include <csignal>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <linux/prctl.h>
#include <memory>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <sched.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <sys/prctl.h>
namespace chrono = std::chrono;

ContainerMonitor::ContainerMonitor() = default;

auto ContainerMonitor::init(const ContainerConfig& config, const std::string& image,
                const std::string& container_name, const Limits& limits, bool is_new) -> void {
        m_container_config = config;
        m_limits = limits;
        m_value_heap = &ValueHeap::get_instance();
        m_log_cmd_queue = &LoggerCommandQueue::get_instance();
        m_pty_session_manager = &PtySessionManager::get_instance();
        m_container_db_manager = &ContainerDbManager::get_instance();
        m_cgroups_manager = CGroupsManagerCreator::create_cgourps_manager(m_container_config.container_id, m_container_config.cgroups_path);
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_log_cmd_queue->map_buffer(Utils::get_logger_command_queue_buf_name(), false);
        if (!m_log_cmd_queue->ok()) [[unlikely]] {
                throw std::runtime_error(m_log_cmd_queue->get_error());
        }
        m_container_db_manager->init();
        if (is_new) {
                ContainerDbObject db_object{};
                db_object.config = std::move(config);
                db_object.image = image;
                db_object.name = container_name.empty() ? std::format("quiver_{}", config.container_id.substr(0, 6)) : container_name;
                db_object.status = "created";
                db_object.boot_time = Utils::get_boot_time();
                db_object.created_at = std::format("{}", chrono::system_clock::now());
                db_object.cpu_quota = limits.cpu_quota;
                db_object.cpu_period = limits.cpu_period;
                db_object.cpu_weight = limits.cpu_weight;
                db_object.memory_max = limits.memory_max;
                db_object.memory_swap = limits.memory_max;
                db_object.pids_limit = limits.pids_limit;
                db_object.cpuset_cpus = limits.cpuset_cpus;
                db_object.cpuset_mems = limits.cpuset_mems;
                db_object.io_max_updates = limits.io_max_updates;
                db_object.io_weight_updates = limits.io_weight_updates;
                m_container_db_manager->add_container(db_object);
        }
}

auto ContainerMonitor::setup_usernamespace() -> void {
        setup_uid_map();
        setup_gid_map();
}

auto ContainerMonitor::start_logging() -> void {
        m_logging_active.store(true, std::memory_order_release);
        m_log_worker = std::jthread([&]() {
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
                                                        std::string log_data{std::format("[{}] [{}] [STDOUT] {}.\n",
                                                                        chrono::system_clock::now(), m_container_config.container_id,
                                                                        std::string_view(buffer, bytes_read))};
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
                                                if (bytes_read > 0) {
                                                        std::string log_data{std::format("[{}] [{}] [STDERR] {}.\n",
                                                                        chrono::system_clock::now(), m_container_config.container_id,
                                                                        std::string_view(buffer, bytes_read))};
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
                        });
}

auto ContainerMonitor::foreground_logging() -> void {
        pollfd fds[2];

        fds[0].fd = m_std_out_fd[0];
        fds[0].events = POLLIN;

        fds[1].fd = m_std_err_fd[0];
        fds[1].events = POLLIN;

        int open_pipes{2};

        while (open_pipes > 0) {
                int ret{poll(fds, 2, -1)};

                if (ret == -1) {
                        if (errno == EINTR)
                                continue;

                        return;
                }

                if (fds[0].fd != -1 && (fds[0].revents & POLLIN)) {
                        char buffer[4096];

                        ssize_t bytes_read{read(fds[0].fd, buffer, sizeof(buffer))};

                        if (bytes_read > 0) {
                                if (!Utils::write_all(STDOUT_FILENO, buffer, bytes_read)) {
                                        return;
                                }
                                std::string log_data{std::format("[{}] [{}] [STDOUT] {}.\n",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                std::string_view(buffer, bytes_read))};
                                this->log_event(log_data, TargetLog::CONTAINERLOG);
                        }
                }

                if (fds[0].fd != -1 && (fds[0].revents & (POLLHUP | POLLERR))) {

                        close(fds[0].fd);
                        fds[0].fd = -1;
                        --open_pipes;
                }

                if (fds[1].fd != -1 && (fds[1].revents & POLLIN)) {
                        char buffer[4096];

                        ssize_t bytes_read{read(fds[1].fd, buffer, sizeof(buffer))};

                        if (bytes_read > 0) {
                                if (!Utils::write_all(STDERR_FILENO, buffer, bytes_read)) {
                                        return;
                                }
                                std::string log_data{std::format("[{}] [{}] [STDOUT] {}.\n",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                std::string_view(buffer, bytes_read))};
                                this->log_event(log_data, TargetLog::CONTAINERLOG);
                        }
                }

                if (fds[1].fd != -1 && (fds[1].revents & (POLLHUP | POLLERR))) {

                        close(fds[1].fd);
                        fds[1].fd = -1;
                        --open_pipes;
                }
        }
}

auto ContainerMonitor::invoke_container() -> void {
        if (m_container_config.terminal.value || m_container_config.detach.value) {
                if (pipe2(m_cli_sync_pipe, O_CLOEXEC) == -1) [[unlikely]] {
                        throw std::runtime_error("Monitor Error: CLI sync pipe creation failed.");
                }
        }
        m_monitor_pid = fork();

        if (m_monitor_pid == -1) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] [{}] Container Monitor Error: fork failed -> {}.\n",
                                        chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno)));
        }
        if (m_monitor_pid > 0) {
                if (m_container_config.terminal.value || m_container_config.detach.value) {
                        close(m_cli_sync_pipe[1]);

                        char status_byte{0};
                        ssize_t n = read(m_cli_sync_pipe[0], &status_byte, 1);
                        close(m_cli_sync_pipe[0]);

                        if (n <= 0) {
                                std::cerr << "Fatal: Container crashed during initialization.\n";
                                exit(EXIT_FAILURE);
                        }

                        std::cout << m_container_config.container_id << "\n";
                        return;
                }
                int status{};
                while (waitpid(m_monitor_pid, &status, 0) == -1) {
                        if (errno == EINTR)
                                continue;
                        return;
                }
                if (WIFEXITED(status)) {
                        exit(WEXITSTATUS(status));
                }
                if (WIFSIGNALED(status)) {
                        exit(128 + WTERMSIG(status));
                }
                return;
        }

        m_monitor_pid = getpid();

        if (m_container_config.detach.value) {
                if (setsid() == -1) [[unlikely]] {
                        std::cerr << "Monitor Fatal: setsid failed.\n";
                        _exit(EXIT_FAILURE);
                }
        }
        if (!attach_to_stdio()) [[unlikely]] {
                std::cerr << "Monitor Fatal: Failed to attach stdio.\n";
                _exit(EXIT_FAILURE);
        }
        if (pipe(m_container_to_monitor_fd) == -1 || pipe(m_monitor_to_container_fd) == -1) [[unlikely]] {
                std::cerr << "Monitor Fatal: pipe creation failed.\n";
                _exit(EXIT_FAILURE);
        }
        ScopeGuard fork_guard{[this]() -> void {
                        close(m_container_to_monitor_fd[0]);
                        close(m_container_to_monitor_fd[1]);
                        close(m_monitor_to_container_fd[0]);
                        close(m_monitor_to_container_fd[1]);
                }
        };
        m_container_pid = fork();
        if (m_container_pid == -1) [[unlikely]] {
                std::cerr << "Monitor Fatal: fork failed.\n";
                _exit(EXIT_FAILURE);
        }
        if (m_container_pid == 0) {
                fork_guard.dismiss();
                run_container_child();
        }
        else {
                fork_guard.dismiss();
                m_container_config.pid = m_container_pid;
                run_monitor_parent();
        }
}

auto ContainerMonitor::run_container_child() -> void {
        close(m_container_to_monitor_fd[0]);
        close(m_monitor_to_container_fd[1]);
        if (prctl(PR_SET_PDEATHSIG, SIGKILL) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: prctl for death signal failed\n",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }

        if (getppid() != m_monitor_pid) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: Monitor died before container\n",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }

        int flags{resolve_namespaces()};
        if (unshare(CLONE_NEWUSER | flags) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: unshare(CLONE_NEWUSER) failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }
        char ready{'1'};
        if (write(m_container_to_monitor_fd[1], &ready, 1) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: write to container_to_monitor_fd failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                close(m_container_to_monitor_fd[1]);
                _exit(EXIT_FAILURE);
        }
        close(m_container_to_monitor_fd[1]);

        char go{};
        if (read(m_monitor_to_container_fd[0], &go, 1) != 1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: read failed from monitor_to_container_fd.\n",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                close(m_monitor_to_container_fd[0]);
                _exit(EXIT_FAILURE);
        }
        close(m_monitor_to_container_fd[0]);
        if (m_container_config.terminal.value) {
                close(m_control_sock[0]);
                m_container_config.control_sock = m_control_sock[1];
        }
        else {
                close(m_std_out_fd[0]);
                close(m_std_err_fd[0]);
                dup2(m_std_out_fd[1], STDOUT_FILENO);
                dup2(m_std_err_fd[1], STDERR_FILENO);
                close(m_std_out_fd[1]);
                close(m_std_err_fd[1]);
                int null_fd{open("/dev/null", O_RDONLY)};
                if (null_fd != -1) {
                        dup2(null_fd, STDIN_FILENO);
                        close(null_fd);
                }
        }
        m_runtime = std::make_unique<ContainerRuntime>(m_container_config);
        m_runtime->run_container();
        _exit(EXIT_FAILURE);
}

auto ContainerMonitor::run_monitor_parent() -> void {
        close(m_monitor_to_container_fd[0]);
        close(m_container_to_monitor_fd[1]);
        char buf{};
        if (read(m_container_to_monitor_fd[0], &buf, 1) != 1) [[unlikely]] {
                std::cerr << "\n[Monitor Error] Child failed during unshare() or setup!\n";
                close(m_container_to_monitor_fd[0]);
                _exit(EXIT_FAILURE);
        }
        close(m_container_to_monitor_fd[0]);

        try {
                setup_usernamespace();
        }
        catch (const std::exception& e) {
                std::cerr << "\n[Monitor Error] User Namespace mapping failed: " << e.what() << "\n";
                _exit(EXIT_FAILURE);
        }
        try {
                m_cgroups_manager->attach_process(m_container_pid);
                if (m_limits.cpu_quota > 0) {
                        m_cgroups_manager->set_cpu_limit(m_limits.cpu_quota, m_limits.cpu_period);
                }
                if (m_limits.cpu_weight > 0) {
                        m_cgroups_manager->set_cpu_weight(m_limits.cpu_weight);
                }
                if (m_limits.memory_max > 0) {
                        m_cgroups_manager->set_memory_max(m_limits.memory_max);
                }
                if (m_limits.memory_swap > 0) {
                        m_cgroups_manager->set_memory_swap(m_limits.memory_swap);
                }
                if (m_limits.pids_limit > 0) {
                        m_cgroups_manager->set_pid_limit(m_limits.pids_limit);
                }
                if (!m_limits.cpuset_cpus.empty()) {
                        m_cgroups_manager->set_cpuset_cpus(m_limits.cpuset_cpus);
                }
                if (!m_limits.cpuset_mems.empty()) {
                        m_cgroups_manager->set_cpuset_mems(m_limits.cpuset_mems);
                }
                for (const auto& im : m_limits.io_max_updates) {
                        m_cgroups_manager->set_io_max(im.major, im.minor, im.limits);
                }
                for (const auto& iw : m_limits.io_weight_updates) {
                        m_cgroups_manager->set_io_weight(iw.major, iw.minor, iw.weight);
                }
        }
        catch (const std::exception& e) {
                log_event(std::format("[{}] {}", chrono::system_clock::now(), e.what()), TargetLog::CONTAINERMON);
                _exit(EXIT_FAILURE);
        }
        ScopeGuard cgroups_guard{[this]() -> void {
                m_cgroups_manager->stop();
                kill(m_container_pid, SIGKILL);
        }};

        bool network_required{true};
        for (const auto& [path, namespace_str] : m_container_config.namespaces) {
                if (namespace_str == "network" && !path.empty()) {
                        network_required = false;
                }
        }
        if (network_required) {
                m_container_config.net_pid = PastaNetwork::setup_networking(m_container_pid, m_container_config.networks);
                bool network_ready{wait_for_network()};

                if (!network_ready) [[unlikely]] {
                        std::cerr << "\n[Monitor Error] Pasta failed to configure network within 1 second.\n";
                        close(m_monitor_to_container_fd[1]);
                        _exit(EXIT_FAILURE);
                }
        }
        ScopeGuard pasta_guard{[this]() -> void {
                if (m_container_config.net_pid > 0)
                        kill(m_container_config.net_pid, SIGTERM);
        }};

        char go_sig{'1'};
        if (write(m_monitor_to_container_fd[1], &go_sig, 1) == -1) [[unlikely]] {
                close(m_monitor_to_container_fd[1]);
                _exit(EXIT_FAILURE);
        }
        close(m_monitor_to_container_fd[1]);

        int master_fd{-1};
        if (m_container_config.terminal.value) {
                close(m_control_sock[1]);

                int slave_fd{-1};
                if (openpty(&master_fd, &slave_fd, nullptr, nullptr, nullptr) == -1) {
                        std::cerr << "openpty failed\n";
                        _exit(EXIT_FAILURE);
                }

                m_pty_session_manager->send_master_fd(m_control_sock[0], slave_fd);
                close(slave_fd);

                if (!m_pty_session_manager->ok()) {
                        _exit(EXIT_FAILURE);
                }

                close(m_control_sock[0]);
        }
        else {
                close(m_std_out_fd[1]);
                close(m_std_err_fd[1]);
        }

        cgroups_guard.dismiss();
        pasta_guard.dismiss();

        auto& signal_handler{SignalHandler::get_instance()};
        signal_handler.set_target_pid(m_container_pid);
        signal_handler.handle_signals();

        auto container{m_container_db_manager->get_container(m_container_config.container_id)};
        if (container) {
                container->config.pid = m_container_pid;
                container->config.final_filesystem = m_container_config.vfs ? Utils::get_vfs_path(m_container_config.container_id).string() :
                        std::format("{}/filesystems/quiver_{}", Utils::get_base_dir().string(), m_container_config.container_id);
                container->boot_time = Utils::get_boot_time();
                container->status = "running";
                m_container_db_manager->update_container(m_container_config.container_id, container.value());
        }

        if (m_container_config.terminal.value || m_container_config.detach.value) {
                char ready_signal{'1'};
                if (write(m_cli_sync_pipe[1], &ready_signal, 1) == -1) {
                        std::cerr << "Monitor Warning: Failed to signal parent CLI.\n";
                }
                close(m_cli_sync_pipe[1]);
        }
        std::atomic<bool> container_running{true};

        std::jthread watchdog_thread{[&container_running]() {
                while (container_running.load(std::memory_order_acquire)) {
                        for (size_t i{0}; i < 100 && container_running.load(std::memory_order_acquire); ++i) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                        if (container_running.load(std::memory_order_acquire)) {
                                Utils::spawn_new_consumer();
                        }
                }
        }};

        ScopeGuard container_teardown_guard{[&]() -> void {
                m_cgroups_manager->stop();
                if (m_container_config.net_pid > 0) {
                        kill(m_container_config.net_pid, SIGTERM);
                }
                container_running.store(false, std::memory_order_release);
                if (watchdog_thread.joinable()) {
                        watchdog_thread.join();
                }
                auto container{m_container_db_manager->get_container(m_container_config.container_id)};
                if (container) {
                        container->status = "exited";
                        m_container_db_manager->update_container(m_container_config.container_id, container.value());
                }
        }};

        if (m_container_config.terminal.value) {
                setup_socket_connection();
                std::jthread([this, master_fd]() {
                        while (true) {
                                int client_fd{accept(m_socket_fd, nullptr, nullptr)};
                                if (client_fd == -1) break;
                                m_pty_session_manager->send_master_fd(client_fd, master_fd);
                                close(client_fd);
                        }
                }).detach();
        }
        else if (!m_container_config.detach.value) {
                foreground_logging();
        }
        else {
                start_logging();
        }

        std::string db_status{"exited"};
        int final_exit_code{EXIT_FAILURE};
        int status{};
        while (waitpid(m_container_pid, &status, 0) == -1) {
                if (errno == EINTR) continue;
                break;
        }

        if (WIFEXITED(status)) {
                final_exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
                int sig{WTERMSIG(status)};
                final_exit_code = 128 + WTERMSIG(status);
        }

        container_teardown_guard.dismiss();

        m_cgroups_manager->stop();
        if (m_container_config.net_pid > 0) {
                kill(m_container_config.net_pid, SIGTERM);
        }

        container_running.store(false, std::memory_order_release);
        if (watchdog_thread.joinable()) {
                watchdog_thread.join();
        }

        auto end_container{m_container_db_manager->get_container(m_container_config.container_id)};
        if (end_container) {
                end_container->status = std::move(db_status);
                m_container_db_manager->update_container(m_container_config.container_id, end_container.value());
        }

        _exit(final_exit_code);
}

auto ContainerMonitor::resolve_namespaces() -> int {
        int flags{};
        for (const auto& [path, namespace_str] : m_container_config.namespaces) {
                auto it{OCIRuntime::NAMESPACE_STR_MAP.find(namespace_str)};
                if (path.empty()) {
                        if (it != OCIRuntime::NAMESPACE_STR_MAP.end()) {
                                flags |= it->second;
                        }
                        else [[unlikely]] {
                                log_event(std::format("[{}] [{}] Container Runtime Error: Unknown or unsupported namespace requested -> {}.\n",
                                                        chrono::system_clock::now(), m_container_config.container_id,
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
                                        log_event(std::format("[{}] [{}] Container Runtime Error: Unknown or unsupported namespace requested -> {}.\n",
                                                                chrono::system_clock::now(), m_container_config.container_id,
                                                                namespace_str), TargetLog::CONTAINERLOG);
                                        _exit(EXIT_FAILURE);
                                }

                                int fd{open(path.c_str(), O_RDONLY | O_CLOEXEC)};
                                if (fd == -1) [[unlikely]] {
                                        log_event(std::format("[{}] [{}] Container Runtime Error: Failed to open namespace path: {}.\n",
                                                                chrono::system_clock::now(), m_container_config.container_id,
                                                                path.string()), TargetLog::CONTAINERLOG);
                                        _exit(EXIT_FAILURE);
                                }

                                if (setns(fd, nstype) == -1) [[unlikely]] {
                                        log_event(std::format("[{}] [{}] Container Runtime Error: setns failed for path: {}. Errno: {}",
                                                                chrono::system_clock::now(), m_container_config.container_id,
                                                                path.string(), errno), TargetLog::CONTAINERLOG);
                                        close(fd);
                                        _exit(EXIT_FAILURE);
                                }
                                close(fd);
                        }
                        else [[unlikely]] {
                                log_event(std::format("[{}] [{}] Container Runtime Error: Namespace path doesn't exist: {}.\n",
                                                        chrono::system_clock::now(), m_container_config.container_id,
                                                        path.string()), TargetLog::CONTAINERLOG);
                                _exit(EXIT_FAILURE);
                        }
                }
        }
        return flags;
}

auto ContainerMonitor::wait_for_network() -> bool {
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
        return network_ready;
}

auto ContainerMonitor::setup_uid_map() -> void {
        std::string username{Utils::get_username()};
        uid_t host_uid{getuid()};
        auto ranges{Utils::parse_subuid(username)};

        if (ranges.empty()) {
                throw std::runtime_error("No subuid range found in /etc/subuid for user");
        }
        std::stringstream payload{""};

        payload << "0 " << host_uid << " 1\n";
        uint32_t container_id{1};
        for (const auto& r : ranges) {
                payload << container_id << " " << r.start << " " << r.count << "\n";
                container_id += r.count;
        }
        const char* newuidmap_path{"/usr/bin/newuidmap"};
        exec_mapping_tool(newuidmap_path, payload.str());
}

auto ContainerMonitor::setup_gid_map() -> void {
        const char* newgidmap_path{"/usr/bin/newgidmap"};
        std::string payload{Utils::build_gid_map_payload(m_container_pid)};
        payload += Utils::get_gid_map_payload(m_container_config.devices);
        exec_mapping_tool(newgidmap_path, payload);
}

auto ContainerMonitor::setup_socket_connection() -> void {
        m_sock_path = Utils::get_sock_path(m_container_config.container_id);
        unlink(m_sock_path.c_str());

        m_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (m_socket_fd == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: socket failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_sock_path.c_str(), sizeof(addr.sun_path)-1);

        if (bind(m_socket_fd, (sockaddr*)&addr, sizeof(addr)) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: bind failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }

        if (listen(m_socket_fd, 1) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: listen failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id), TargetLog::CONTAINERLOG);
                _exit(EXIT_FAILURE);
        }
}

auto ContainerMonitor::attach_to_container(const std::string& container_id) -> void {
        m_sock_path = Utils::get_sock_path(container_id);

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_sock_path.c_str(), sizeof(addr.sun_path)-1);

        int retries{0};
        while (true) {
                if (m_connection_fd != -1) close(m_connection_fd);
                m_connection_fd = socket(AF_UNIX, SOCK_STREAM, 0);

                if (connect(m_connection_fd, (sockaddr*)&addr, sizeof(addr)) == 0) {
                        break;
                }

                if (errno != ENOENT && errno != ECONNREFUSED) {
                        std::cerr << std::format("[{}] Container Runtime Error: connect failed -> {}.\n",
                                container_id, std::strerror(errno));
                        _exit(EXIT_FAILURE);
                }

                if (retries++ > 100) {
                        std::cerr << std::format("[{}] Container Runtime Error: Timed out connecting to socket.\n",
                                container_id);
                        _exit(EXIT_FAILURE);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        int master_fd{m_pty_session_manager->recv_master_fd(m_connection_fd)};
        close(m_connection_fd);
        m_connection_fd = -1;

        if (!m_pty_session_manager->ok() || master_fd == -1) {
                std::cerr << "Container Runtime Error: Failed to receive PTY master fd.\n";
                _exit(EXIT_FAILURE);
        }

        m_pty_session_manager->enable_raw_mode();

        struct winsize ws{};
        if (m_container_config.console_size.width > 0 && m_container_config.console_size.height > 0) {
                ws.ws_col = static_cast<unsigned short>(m_container_config.console_size.width);
                ws.ws_row = static_cast<unsigned short>(m_container_config.console_size.height);
                ioctl(master_fd, TIOCSWINSZ, &ws);
        }
        else {
                if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
                        ioctl(master_fd, TIOCSWINSZ, &ws);
                }
        }

        int sigwinch_pipe[2];
        if (pipe2(sigwinch_pipe, O_NONBLOCK | O_CLOEXEC) == -1) {
                close(master_fd);
                m_pty_session_manager->disable_raw_mode();
                std::cerr << "Container Runtime Error: pipe2 failed.\n";
                return;
        }

        static int s_sigwinch_write_fd{-1};
        s_sigwinch_write_fd = sigwinch_pipe[1];

        struct sigaction sa{};
        sa.sa_handler = [](int) {
                char c{'W'};
                (void)write(s_sigwinch_write_fd, &c, 1);
        };
        sa.sa_flags = 0;
        sigaction(SIGWINCH, &sa, nullptr);

        pollfd fds[3];
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = master_fd;
        fds[1].events = POLLIN;
        fds[2].fd = sigwinch_pipe[0];
        fds[2].events = POLLIN;

        bool running{true};
        bool is_detached{false};
        bool saw_ctrl_p{false};

        while (running) {
                int n_ready{poll(fds, 3, -1)};
                if (n_ready == -1) {
                        if (errno == EINTR) continue;
                        break;
                }

                if (fds[2].revents & POLLIN) {
                        char drain[64];
                        while (read(sigwinch_pipe[0], drain, sizeof(drain)) > 0) {}
                        if (m_container_config.console_size.width > 0 && m_container_config.console_size.height > 0) {
                                ioctl(master_fd, TIOCSWINSZ, &ws);
                        }
                        else {
                                if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
                                        ioctl(master_fd, TIOCSWINSZ, &ws);
                                }
                        }
                }

                if (fds[0].revents & POLLIN) {
                        char buf[4096];
                        ssize_t n{read(STDIN_FILENO, buf, sizeof(buf))};
                        if (n == 0) { running = false; break; }
                        if (n < 0) {
                                if (errno == EAGAIN || errno == EINTR) {}
                                else { running = false; break; }
                        } else {
                                std::vector<char> out{};
                                out.reserve(n);
                                for (ssize_t i{0}; i < n; ++i) {
                                        unsigned char c{static_cast<unsigned char>(buf[i])};
                                        if (saw_ctrl_p) {
                                                saw_ctrl_p = false;
                                                if (c == CTRL_Q) {
                                                        is_detached = true;
                                                        running = false;
                                                        break;
                                                }
                                                out.push_back(CTRL_P);
                                        }
                                        if (c == CTRL_P) {
                                                saw_ctrl_p = true;
                                                continue;
                                        }
                                        out.push_back(static_cast<char>(c));
                                }
                                if (!out.empty()) {
                                        if (!Utils::write_all(master_fd, out.data(), out.size())) {
                                                running = false;
                                        }
                                }
                        }
                }

                if (fds[1].revents & POLLIN) {
                        char buf[4096];
                        ssize_t n{read(master_fd, buf, sizeof(buf))};
                        if (n == 0) {
                        } else if (n < 0) {
                                if (errno == EAGAIN || errno == EINTR) {}
                                else { running = false; break; }
                        } else {
                                if (!Utils::write_all(STDOUT_FILENO, buf, n)) {
                                        running = false;
                                }
                                std::string log_data{std::format("[{}] [{}] [PTY] {}.\n",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                std::string_view(buf, n))};
                                this->log_event(log_data, TargetLog::CONTAINERLOG);
                        }
                }

                if ((fds[1].revents & POLLHUP) || (fds[1].revents & POLLERR)) {
                        char buf[4096];
                        ssize_t n;
                        while ((n = read(master_fd, buf, sizeof(buf))) > 0) {
                                Utils::write_all(STDOUT_FILENO, buf, n);
                        }
                        running = false;
                }
        }

        close(master_fd);
        close(sigwinch_pipe[0]);
        close(sigwinch_pipe[1]);

        m_pty_session_manager->disable_raw_mode();
        if (is_detached) {
                std::cerr << "[Detached]\n";
        }
        else {
                std::cerr << "[Session Terminated]\n";
        }
}

auto ContainerMonitor::exec_mapping_tool(const char* binary_path, const std::string& payload) -> void {
        pid_t pid{fork()};

        if (pid == -1) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] [{}] Container Monitor Error: fork failed for {} -> {}.\n",
                                        chrono::system_clock::now(), m_container_config.container_id,
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

                std::cerr << std::format("[{}] [{}] Container Monitor Fatal: execl failed for {} -> {}.\n",
                                chrono::system_clock::now(), m_container_config.container_id, binary_path, std::strerror(errno));
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
                                throw std::runtime_error(std::format("Container Monitor Error: {} failed with user exit code {}.\n",
                                                        binary_path, exit_code));
                        }
                }
                else if (WIFSIGNALED(status)) {
                        throw std::runtime_error(std::format("Container Monitor Error: {} was terminated by signal {}.\n",
                                                binary_path, WTERMSIG(status)));
                }
                destroy = true;
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
                std::this_thread::sleep_for(chrono::milliseconds(1));
        }
}

ContainerMonitor::~ContainerMonitor() {
        if (destroy) {
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
}
