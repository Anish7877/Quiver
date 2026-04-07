#include "container_runtime.hpp"
#include "caps_manager.hpp"
#include "logger_command_queue.hpp"
#include "mount.hpp"
#include "oci_runtime.hpp"
#include "schedular.hpp"
#include "seccomp_profile_manager.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <linux/prctl.h>
#include <memory>
#include <sched.h>
#include <stdexcept>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <grp.h>
#include <thread>
#include <unistd.h>
#include <chrono>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <iostream>
namespace chrono = std::chrono;

ContainerRuntime::ContainerRuntime(const ContainerConfig& config) : m_container_config{config} {
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
        setup_security_paths();
        setup_environment_variables();
        try {
                Schedular::apply_opts(m_container_config.schedular_opts);
                m_caps_manager->apply();
                if (m_container_config.no_new_privileges.value) {
                        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) [[unlikely]] {
                                log_event(std::format("[{}] [{}] Container Runtime Error: prctl set no privileges failed.",
                                                        chrono::system_clock::now(), m_container_config.container_id));
                                _exit(EXIT_FAILURE);
                        }
                }
                m_seccomp_profile_manager->apply();
        }
        catch (const std::exception& e) {
                log_event(std::format("[{}] [{}] {}", chrono::system_clock::now(), m_container_config.container_id, e.what()));
                _exit(EXIT_FAILURE);
        }
        execl("/bin/bash", "/bin/bash", static_cast<char*>(NULL));
        log_event(std::format("[{}] [{}] Container Runtime Error: execl failed.",
                                chrono::system_clock::now(), m_container_config.container_id));
        _exit(EXIT_FAILURE);
}

auto ContainerRuntime::setup_root_filesystem() -> void {
        int prop_flags{-1};
        fs::path final_filesystem{};
        if (m_container_config.rootfs_propagation.type == "shared") {
                m_container_config.rootfs_propagation.type = "slave";
        }
        auto it{OCIRuntime::ROOTFS_PROPAGATION_STR_MAP.find(m_container_config.rootfs_propagation.type)};
        if (it != OCIRuntime::ROOTFS_PROPAGATION_STR_MAP.end()) {
                prop_flags = it->second;
        }
        else [[unlikely]] {
                prop_flags = MS_PRIVATE;
        }

        if (!Mount::_set_propagation("/", prop_flags)) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: root mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if(!m_container_config.vfs) {
                fs::path lower_dir{m_container_config.rootfs.path};
                fs::path upper_dir{Utils::get_base_dir() / "filesystems"
                        / std::format("quiver_{}", m_container_config.container_id) / "upper_dir"};
                fs::path work_dir{Utils::get_base_dir() / "filesystems"
                        / std::format("quiver_{}", m_container_config.container_id) / "work_dir"};
                fs::path merged_dir{Utils::get_base_dir() / "filesystems"
                        / std::format("quiver_{}", m_container_config.container_id) / "merged_dir"};
                try {
                        Utils::ensure_dir(upper_dir);
                        Utils::ensure_dir(work_dir);
                        Utils::ensure_dir(merged_dir);
                }
                catch (const std::exception& e) {
                        log_event(std::format("[{}] [{}] {}", chrono::system_clock::now(), m_container_config.container_id, e.what()));
                        _exit(EXIT_FAILURE);
                }
                std::string opts{std::format("lowerdir={},upperdir={},workdir={}", lower_dir.string(), upper_dir.string(), work_dir.string())};
                if(!Mount::_overlay_fs(merged_dir, opts)) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: overlay filesystem mount failed.",
                                                chrono::system_clock::now(), m_container_config.container_id));
                        _exit(EXIT_FAILURE);
                }
                final_filesystem = merged_dir;
        }
        else {
                fs::path dest{Utils::get_base_dir() / "vfs" / std::format("quiver_{}", m_container_config.container_id)};
                try {
                        Utils::copy_directory(m_container_config.rootfs.path, dest);
                }
                catch (const std::exception& e) {
                        log_event(std::format("[{}] [{}] {}", chrono::system_clock::now(), m_container_config.container_id, e.what()));
                        _exit(EXIT_FAILURE);
                }
                final_filesystem = dest;
        }

        if (!Mount::_new_filesystem(final_filesystem)) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: new filesystem mount failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (chdir(final_filesystem.c_str()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir to new filesystem failed.",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        Mount::_volumes(m_container_config.mounts, final_filesystem);
        Mount::_devices(m_container_config.devices, final_filesystem);
}

auto ContainerRuntime::jail_process() -> void {
        Utils::ensure_dir("oldroot");

        if (syscall(SYS_pivot_root, ".", "oldroot") == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: pivot root failed with error {}.",
                                        chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno)));
                _exit(EXIT_FAILURE);
        }

        if (chdir("/") == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir / failed.",
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
        if (chdir(m_container_config.cwd.value.c_str()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir to cwd failed.",
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
                _exit(EXIT_FAILURE);
        }

        for (const auto& env : m_container_config.env.value) {
                auto it{env.find('=')};
                if (it != std::string::npos && it > 0) {
                        std::string name{env.substr(0, it)};
                        std::string value{env.substr(it+1)};
                        if (setenv(name.c_str(), value.c_str(), 1) != 0) [[unlikely]] {
                                log_event(std::format("[{}] [{}] Container Runtime Error: setenv failed for -> {}.",
                                                        chrono::system_clock::now(), m_container_config.container_id,
                                                        env));
                        }
                }
                else [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: Invalid env format ignored -> {}.",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                env));
                }
        }
}

auto ContainerRuntime::setup_security_paths() -> void {
        for (const auto& path : m_container_config.masked_paths.paths) {
                if (access(path.c_str(), F_OK) != 0) {
                        continue;
                }

                if (fs::is_directory(path)) {
                        if (mount("tmpfs", path.c_str(), "tmpfs", MS_RDONLY, "ro,mode=755") == -1) [[unlikely]] {
                                log_event(std::format("[{}] [{}] Container Runtime Error: Failed to mask directory {} -> {}",
                                                        chrono::system_clock::now(), m_container_config.container_id,
                                                        path.string(), std::strerror(errno)));
                                _exit(EXIT_FAILURE);
                        }
                }
                else {
                        if (mount("/dev/null", path.c_str(), "", MS_BIND, NULL) == -1) [[unlikely]] {
                                log_event(std::format("[{}] [{}] Container Runtime Error: Failed to mask file {} -> {}",
                                                        chrono::system_clock::now(), m_container_config.container_id,
                                                        path.string(), std::strerror(errno)));
                                _exit(EXIT_FAILURE);
                        }
                }
        }

        for (const auto& path : m_container_config.read_only_paths.paths) {
                if (access(path.c_str(), F_OK) != 0) {
                        continue;
                }

                if (mount(path.c_str(), path.c_str(), "", MS_BIND | MS_REC, NULL) == -1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: Failed to bind-mount readonly path {} -> {}",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                path.string(), std::strerror(errno)));
                        _exit(EXIT_FAILURE);
                }

                if (mount(path.c_str(), path.c_str(), "", MS_BIND | MS_REMOUNT | MS_RDONLY | MS_REC, NULL) == -1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: Failed to remount readonly path {} -> {}",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                path.string(), std::strerror(errno)));
                        _exit(EXIT_FAILURE);
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
