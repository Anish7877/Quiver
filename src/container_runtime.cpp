#include "caps_manager.hpp"
#include "container_runtime.hpp"
#include "logger_command_queue.hpp"
#include "mount.hpp"
#include "oci_runtime.hpp"
#include "pty_session_manager.hpp"
#include "schedular.hpp"
#include "seccomp_profile_manager.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include <asm-generic/ioctls.h>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <grp.h>
#include <iostream>
#include <linux/prctl.h>
#include <memory>
#include <pwd.h>
#include <sched.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace chrono = std::chrono;

ContainerRuntime::ContainerRuntime(const ContainerConfig& config) : m_container_config{config} {
        m_value_heap = &ValueHeap::get_instance();
        m_log_cmd_queue = &LoggerCommandQueue::get_instance();
        m_pty_session_manager = &PtySessionManager::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] Container Runtime Error: failed to map value heap.\n",
                                        m_container_config.container_id));
        }
        m_log_cmd_queue->map_buffer(Utils::get_logger_command_queue_buf_name(), false);
        if (!m_log_cmd_queue->ok()) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] Container Runtime Error: failed to map log command queue.\n",
                                        m_container_config.container_id));
        }
        try {
                m_caps_manager = std::make_unique<CapsManager>(m_container_config.capabilities);
                if (!m_container_config.seccomp.default_action.empty()) {
                        m_seccomp_profile_manager = std::make_unique<SeccompProfileManager>(m_container_config.seccomp);
                }
        }
        catch (const std::exception& e) {
                log_event(std::format("[{}] [{}] {}", chrono::system_clock::now(), m_container_config.container_id, e.what()));
                _exit(EXIT_FAILURE);
        }
}


auto ContainerRuntime::run_container() -> void {
        pid_t container_init{fork()};
        if (container_init == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: fork failed for container init.\n",
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

auto ContainerRuntime::exec_commands() -> void {
        std::vector<char*> c_args{};
        for (const auto& arg : m_container_config.args.value) {
                c_args.emplace_back(const_cast<char*>(arg.c_str()));
        }
        c_args.emplace_back(nullptr);

        execvp(m_container_config.args.value[0].c_str(), c_args.data());
}

auto ContainerRuntime::execute_container_init() -> void {
        if (m_container_config.terminal.value) {
                int slave_fd{m_pty_session_manager->recv_master_fd(m_container_config.control_sock)};
                if (!m_pty_session_manager->ok() || slave_fd == -1) {
                        log_event("Container Runtime Error: Failed to receive slave_fd\\n");
                        _exit(EXIT_FAILURE);
                }
                close(m_container_config.control_sock);

                if (setsid() == -1) {
                        log_event("Container Runtime Error: setsid failed.\\n");
                        _exit(EXIT_FAILURE);
                }

                if (ioctl(slave_fd, TIOCSCTTY, 0) == -1) {
                        log_event(std::format(
                                                "[{}] [{}] Container Runtime Error: TIOCSCTTY failed -> {}.\\n",
                                                chrono::system_clock::now(),
                                                m_container_config.container_id,
                                                std::strerror(errno)
                                             ));
                        _exit(EXIT_FAILURE);
                }

                dup2(slave_fd, STDIN_FILENO);
                dup2(slave_fd, STDOUT_FILENO);
                dup2(slave_fd, STDERR_FILENO);

                if (slave_fd > STDERR_FILENO) {
                        close(slave_fd);
                }
        }

        passwd* pw{getpwuid(m_container_config.user.uid)};
        if (!pw) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: getpwuid failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (initgroups(pw->pw_name, pw->pw_gid) == -1) {
                _exit(EXIT_FAILURE);
        }
        if (setgid(0) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: setgid failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (setuid(0) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: setuid failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (sethostname(m_container_config.hostname.c_str(), m_container_config.hostname.length()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: sethostname failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (setdomainname(m_container_config.domain_name.c_str(), m_container_config.domain_name.length()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: setdomainname failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        setup_root_filesystem();
        mount_necessary_dirs();
        jail_process();
        setup_standard_symlinks();
        apply_rlimits();
        setup_security_paths();
        setup_environment_variables();
        try {
                Schedular::apply_opts(m_container_config.schedular_opts);
                m_caps_manager->apply();
                if (m_container_config.no_new_privileges.value) {
                        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) [[unlikely]] {
                                log_event(std::format("[{}] [{}] Container Runtime Error: prctl set no new privileges failed.\n",
                                                        chrono::system_clock::now(), m_container_config.container_id));
                                _exit(EXIT_FAILURE);
                        }
                }
                if (m_seccomp_profile_manager) {
                        m_seccomp_profile_manager->apply();
                }
                if (m_container_config.oom_score.value >= -1000 && m_container_config.oom_score.value <= 1000) {
                }
        }
        catch (const std::exception& e) {
                log_event(std::format("[{}] [{}] {}", chrono::system_clock::now(), m_container_config.container_id, e.what()));
                _exit(EXIT_FAILURE);
        }

        if (setuid(m_container_config.user.uid) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: final setuid failed before exec.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (setgid(m_container_config.user.gid) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: final setgid failed before exec.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (setgroups(m_container_config.user.additional_gids.size(), m_container_config.user.additional_gids.data()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: final setgroups failed before exec.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (chdir(m_container_config.cwd.value.c_str()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir to workdir failed before exec.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (umask(m_container_config.user.umask) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: umask failed before exec.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        exec_commands();
        log_event(std::format("[{}] [{}] Container Runtime Error: exec failed.\n",
                                chrono::system_clock::now(), m_container_config.container_id));
        _exit(EXIT_FAILURE);
}

auto ContainerRuntime::setup_root_filesystem() -> void {
        int prop_flags{-1};
        bool is_overlay_mounted{false};
        int overlay_err{-1};
        fs::path final_filesystem{};
        std::string propagation_type{m_container_config.rootfs_propagation.type};
        if (propagation_type == "shared") {
                propagation_type = "slave";
        }
        auto it{OCIRuntime::ROOTFS_PROPAGATION_STR_MAP.find(propagation_type)};
        if (it != OCIRuntime::ROOTFS_PROPAGATION_STR_MAP.end()) {
                prop_flags = it->second;
        }
        else [[unlikely]] {
                prop_flags = MS_PRIVATE;
        }

        if (!Mount::_set_propagation("/", prop_flags)) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: root mount failed.\n",
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
                        log_event(std::format("[{}] [{}] {}", chrono::system_clock::now(),
                                                m_container_config.container_id, e.what()));
                        _exit(EXIT_FAILURE);
                }
                std::string opts{std::format("lowerdir={},upperdir={},workdir={}", lower_dir.string(),
                                              upper_dir.string(), work_dir.string())};
                if (mount("overlay", merged_dir.c_str(), "overlay", 0,opts.c_str()) == 0) {
                        is_overlay_mounted = true;
                }
                else {
                        overlay_err = errno;
                        log_event(std::format("[{}] [{}] OverlayFS mount failed -> {}.\n", chrono::system_clock::now(),
                                                m_container_config.container_id, std::strerror(errno)));
                }
                if (!is_overlay_mounted) {
                        if (overlay_err == EPERM || overlay_err == EINVAL || overlay_err == ENODEV || overlay_err == EOPNOTSUPP) {
                                log_event(std::format("[{}] [{}] Falling back to FUSE overlayfs.\n", chrono::system_clock::now(),
                                                        m_container_config.container_id));
                                pid_t fuse_pid{fork()};
                                if (fuse_pid == -1) [[unlikely]] {
                                        log_event(std::format("[{}] [{}] Container Runtime Fatal: FUSE overlayfs fork failed -> {}.\n",
                                                                chrono::system_clock::now(),m_container_config.container_id,
                                                                std::strerror(errno)));
                                }
                                if (fuse_pid == 0) {
                                        execlp("fuse-overlayfs", "fuse-overlayfs", "-o", opts.c_str(), merged_dir.c_str(),nullptr);
                                        _exit(EXIT_FAILURE);
                                }

                                bool mounted{false};
                                for (size_t i{0}; i < 100; ++i) {
                                        std::ifstream mounts("/proc/self/mountinfo");
                                        std::string line{};
                                        while (std::getline(mounts, line)) {
                                                if (line.find(merged_dir.string()) != std::string::npos
                                                                && line.find("fuse-overlayfs") != std::string::npos) {
                                                        mounted = true;
                                                        break;
                                                }
                                        }
                                        if (mounted) {
                                                is_overlay_mounted = true;
                                        }
                                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                }
                                if (!mounted) {
                                        log_event(std::format("[{}] [{}] Container Runtime Fatal: fuse-overlayfs mount verification failed.\n",
                                                                chrono::system_clock::now(), m_container_config.container_id));
                                        _exit(EXIT_FAILURE);
                                }
                        }
                        else {
                                        log_event(std::format("[{}] [{}] Container Runtime Fatal: Filesystem mount failed.\n",
                                                                chrono::system_clock::now(), m_container_config.container_id));
                                        _exit(EXIT_FAILURE);
                        }
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
                log_event(std::format("[{}] [{}] Container Runtime Error: new filesystem mount failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (chdir(final_filesystem.c_str()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir to new filesystem failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        Mount::_volumes(m_container_config.mounts, final_filesystem);
        Mount::_devices(m_container_config.devices, final_filesystem);
}

auto ContainerRuntime::jail_process() -> void {
        if (mount(".", ".", "bind", MS_BIND | MS_REC, NULL) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: bind mount for pivot_root failed -> {}.\n",
                                        chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno)));
                _exit(EXIT_FAILURE);
        }

        Utils::ensure_dir("oldroot");

        if (syscall(SYS_pivot_root, ".", "oldroot") == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: pivot root failed with error {}.\n",
                                        chrono::system_clock::now(), m_container_config.container_id, std::strerror(errno)));
                _exit(EXIT_FAILURE);
        }

        if (chdir("/") == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir / failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (!Mount::_unmount_filesystem("oldroot")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: umount oldroot failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (rmdir("oldroot") == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: rmdir oldroot failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        if (chdir(m_container_config.cwd.value.c_str()) == -1) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: chdir to cwd failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
}

auto ContainerRuntime::mount_necessary_dirs() -> void {
        try {
                Utils::ensure_dir("./proc");
                Utils::ensure_dir("./sys");
                Utils::ensure_dir("./dev");
                Utils::ensure_dir("./tmp");
                Utils::ensure_dir("./run");
        }
        catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
        }

        if (!Mount::_proc()) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: proc mount failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_sys()) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: sys mount failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_dev()) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: dev mount failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        Utils::ensure_dir("./dev/shm");
        Utils::ensure_dir("./dev/pts");

        struct { const char* host_path; const char* container_path; } essential_devices[] = {
                {"/dev/null",    "./dev/null"},
                {"/dev/zero",    "./dev/zero"},
                {"/dev/full",    "./dev/full"},
                {"/dev/random",  "./dev/random"},
                {"/dev/urandom", "./dev/urandom"},
        };
        for (const auto& dev : essential_devices) {
                int fd = open(dev.container_path, O_CREAT | O_RDONLY, 0666);
                if (fd != -1) close(fd);
                if (mount(dev.host_path, dev.container_path, NULL, MS_BIND, NULL) == -1) {
                        log_event(std::format("[{}] [{}] Container Runtime Warning: Failed to bind-mount {} -> {}.\n",
                                        chrono::system_clock::now(), m_container_config.container_id,
                                        dev.host_path, std::strerror(errno)));
                }
        }

        if (!Mount::_tmpfs("./tmp", "mode=1777")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: tmp mount failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_tmpfs("./run", "mode=0755")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: run mount failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_tmpfs("./dev/shm", "mode=1777,size=65536k")) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: shm mount failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }
        if (!Mount::_devpts("./dev/pts", "newinstance,ptmxmode=0666,mode=0620")) [[unlikely]] {
                _exit(EXIT_FAILURE);
        }
}

auto ContainerRuntime::apply_rlimits() -> void {
        for (const auto& rlimit : m_container_config.rlimits) {
                int resource_flag{};
                auto it{OCIRuntime::RLIMIT_STR_MAP.find(rlimit.name)};
                if (it != OCIRuntime::RLIMIT_STR_MAP.end()) {
                        resource_flag = it->second;
                }
                else [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: Unknown rlimits resource found -> '{}'.\n",
                                                chrono::system_clock::now(), m_container_config.container_id, rlimit.name));
                        continue;
                }
                struct rlimit limit{};
                limit.rlim_cur = rlimit.soft_limit;
                limit.rlim_max = rlimit.hard_limit;
                if (setrlimit(resource_flag, &limit) == -1) [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: Unable to set resource limit for '{}' -> '{}'.\n",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                rlimit.name, std::strerror(errno)));
                }
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
                        log_event(std::format("[{}] [{}] Container Runtime Error: Failed to symlink {} to {} -> {}.\n",
                                                chrono::system_clock::now(), m_container_config.container_id,
                                                link.first, link.second, std::strerror(errno)));
                }
        }
}

auto ContainerRuntime::setup_environment_variables() -> void {
       if (clearenv() != 0) [[unlikely]] {
                log_event(std::format("[{}] [{}] Container Runtime Error: clearenv failed.\n",
                                        chrono::system_clock::now(), m_container_config.container_id));
                _exit(EXIT_FAILURE);
        }

        for (const auto& env : m_container_config.env.value) {
                auto it{env.find('=')};
                if (it != std::string::npos && it > 0) {
                        std::string name{env.substr(0, it)};
                        std::string value{env.substr(it+1)};
                        if (setenv(name.c_str(), value.c_str(), 1) != 0) [[unlikely]] {
                                log_event(std::format("[{}] [{}] Container Runtime Error: setenv failed for -> {}.\n",
                                                        chrono::system_clock::now(), m_container_config.container_id,
                                                        env));
                        }
                }
                else [[unlikely]] {
                        log_event(std::format("[{}] [{}] Container Runtime Error: Invalid env format ignored -> {}.\n",
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
                log_event(std::format("[{}] [{}] Container Runtime Error: waitpid failed for container init.\n",
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

ContainerRuntime::~ContainerRuntime() = default;
