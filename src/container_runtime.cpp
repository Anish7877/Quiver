#include "container_runtime.hpp"
#include "caps_manager.hpp"
#include "logger_command_queue.hpp"
#include "mount.hpp"
#include "seccomp_profile_manager.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <sched.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <grp.h>
#include <thread>
#include <unistd.h>
#include <chrono>
#include <sys/wait.h>
#include <iostream>
namespace chrono = std::chrono;

ContainerRuntime::ContainerRuntime(const ContainerConfig& config) {
        m_container_config = config;
        m_value_heap = &ValueHeap::get_instance();
        m_log_cmd_queue = &LoggerCommandQueue::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] Container Runtime Error: failed to map value heap.",
                                        m_container_config.container_id));
        }
        m_log_cmd_queue->map_buffer(Utils::get_logger_command_queue_buf_name(), false);
        if (!m_log_cmd_queue->ok()) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] Container Runtime Error: failed to map log command queue.",
                                        m_container_config.container_id));
        }
        try {
                m_caps_manager = std::make_unique<CapsManager>(m_container_config.capabilities);
                m_seccomp_profile_manager = std::make_unique<SeccompProfileManager>(m_container_config.seccomp);
        }
        catch (const std::exception& e) {
                log_event(std::format("[{}] [{}] {}", chrono::system_clock::now(), m_container_config.container_id, e.what()));
                _exit(EXIT_FAILURE);
        }
}

auto ContainerRuntime::exec_commands() -> void {
        // TODO: get the commands from parsed manifest and run it inside the container namespace
}

auto ContainerRuntime::pause_container() -> void {
        // TODO: store a checkpoint from where we can resume the container and stop the container
}

auto ContainerRuntime::unpause_container() -> void {
        // TODO: load the checkpoint stored in the pause and restart the container from that point
}

auto ContainerRuntime::restart_container() -> void {
        // TODO: restart container with the default config
}

auto ContainerRuntime::run_container() -> void {
        int flags{};
        if (unshare(flags) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: unshare failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if(setgroups(0, NULL) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: setgroups failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (setuid(0) == -1 || setgid(0) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: setuid or setgid failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        pid_t container_init{fork()};
        if (container_init == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: fork failed for container init.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        else if (container_init == 0) {
                execute_container_init();
        }
        else {
                supervise_container(container_init);
        }
}

auto ContainerRuntime::execute_container_init() -> void {
        if (setsid() == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: setsid failed for container init.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (sethostname(m_container_config.hostname.c_str(), m_container_config.hostname.length()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: sethostname failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (setdomainname(m_container_config.domain_name.c_str(), m_container_config.domain_name.length()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: setdomainname failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        setup_root_filesystem();
        jail_process();
        mount_necessary_dirs();
        setup_standard_symlinks();
        try {
                m_caps_manager->apply();
                m_seccomp_profile_manager->apply();
        }
        catch (const std::exception& e) {
                log_event(std::format("[{}] [{}] {}", chrono::system_clock::now(), m_container_config.container_id, e.what()));
        }
        execl("/bin/bash", "/bin/bash", static_cast<char*>(NULL));
        log_event(std::format("[{}] [{}] Container Runtime Error: execl failed.",
                                chrono::system_clock::now(), m_container_config.container_id));
        _exit(EXIT_FAILURE);
}

auto ContainerRuntime::setup_root_filesystem() -> void {
        if (!Mount::_private("/")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: root mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }


        if(!m_container_config.vfs) {
                if(!Mount::_overlay_fs(m_container_config.rootfs.path, "")) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: overlay filesystem mount failed.",
                                                chrono::system_clock::now(), m_container_config.container_id));
                        _exit(EXIT_FAILURE);
                }
        }

        if (!Mount::_new_filesystem(m_container_config.rootfs.path)) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: new filesystem mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (chdir(m_container_config.rootfs.path.c_str()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir to new filesystem failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (!Mount::_volumes(m_container_config.volumes)) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: volumes mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (!Mount::_devices(m_container_config.)) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: devices mount failed for container init.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
}

auto ContainerRuntime::jail_process() -> void {
        Utils::ensure_dir("oldroot");

        if (syscall(SYS_pivot_root, ".", "oldroot") == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: pivot root failed with error {}.",
                                        chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno)));
                _exit(EXIT_FAILURE);
        }

        if (chdir(m_container_config.cwd.value.c_str()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (!Mount::_unmount_filesystem("oldroot")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: umount oldroot failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (rmdir("oldroot") == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: rmdir oldroot failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
}

auto ContainerRuntime::mount_necessary_dirs() -> void {
        try {
                Utils::ensure_dir("/proc");
                Utils::ensure_dir("/sys");
                Utils::ensure_dir("/dev");
                Utils::ensure_dir("/tmp");
                Utils::ensure_dir("/run");
        }
        catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
        }

        if (!Mount::_proc()) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: proc mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_sys()) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: sys mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_dev()) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: dev mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        Utils::ensure_dir("/dev/shm");
        Utils::ensure_dir("/dev/pts");

        if (!Mount::_tmpfs("/tmp", "mode=1777")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: tmp mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_tmpfs("/run", "mode=0755")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: run mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_tmpfs("/dev/shm", "mode=1777,size=65536k")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: shm mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_devpts("/dev/pts", "newinstance,ptmxmode=0666,mode=0620,gid=5")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: devpts mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
}

auto ContainerRuntime::setup_standard_symlinks() -> void {
        std::vector<std::pair<std::string, std::string>> links{
                {"/proc/self/fd",   "/dev/fd"},
                {"/proc/self/fd/0", "/dev/stdin"},
                {"/proc/self/fd/1", "/dev/stdout"},
                {"/proc/self/fd/2", "/dev/stderr"},
                {"pts/ptmx",        "/dev/ptmx"}
        };

        for (const auto& link : links) {
                unlink(link.second.c_str());

                if (symlink(link.first.c_str(), link.second.c_str()) == -1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: Failed to symlink {} to {} -> {}.",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                link.first, link.second, std::strerror(errno)));
                }
        }
}

auto ContainerRuntime::setup_environment_variables() -> void {
        if (clearenv() != 0) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: clearenv failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                return;
        }
        const auto& names{m_container_config.env.name};
        const auto& values{m_container_config.env.value};
        for (std::size_t i{0}; i < names.size(); ++i) {
                if (setenv(names[i], values[i], 1) == -1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: setenv failed for name: {} -> value: {}.",
                                        chrono::system_clock::now(), m_container_config.container_id,
                                        names[i], values[i]));
                }
        }
}

auto ContainerRuntime::supervise_container(pid_t pid) -> void {
        int status{};
        while (waitpid(pid, &status, 0) == -1) {
                if (errno == EINTR) continue;
                log_event(std::format("[{}] [{}] Container Runtime Error: waitpid failed for container init.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
                _exit(WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
                _exit(128 + WTERMSIG(status));
        }
        _exit(EXIT_FAILURE);
}

auto ContainerRuntime::log_event(const std::string& log_data) -> void {
        std::size_t offset{};
        while (!m_value_heap->write_job_data(log_data, offset)) {
                std::this_thread::yield();
        }
        m_log_job_data.target_log = TargetLog::CONTAINERLOG;
        m_log_job_data.value_offset = offset;
        m_log_job_data.value_length = log_data.size();
        while (!m_log_cmd_queue->atomic_push(m_log_job_data)) {
                std::this_thread::yield();
        }
}
