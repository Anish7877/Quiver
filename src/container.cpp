#include "../include/container.hpp"
#include "../include/utils.hpp"
#include "../include/network.hpp"
#include "../include/package_manager.hpp"
#include "../include/mount.hpp"
#include "../include/device_manager.hpp"
#include <cstdlib>
#include <iostream>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <cstring>

pid_t Container::m_child_pid{ -1 };
std::string Container::m_new_hostname{ "container" };
std::string Container::m_new_fs{ "" };
std::vector<std::string> Container::m_volumes{};
std::vector<std::string> Container::m_commands{};
Terminal Container::m_term{};
Terminal::PtyArgs Container::m_pty_args{};

Container::Container(const std::string& hostname, const std::string& new_fs, const std::vector<std::string>& volumes, DatabaseManager& db, const std::string& container_id)
    : m_db(&db), m_container_id(container_id) {
    m_new_hostname = hostname;
    m_new_fs = new_fs;
    m_volumes = volumes;
}

void Container::exec(const std::string& program_path, const std::vector<std::string>& commands){
    m_commands = commands;
    run(program_path, m_container_id);
}

void Container::set_filesystem(const std::string& path){
    m_new_fs = path;
}

void Container::connect_to_server(const pid_t& container_pid){
    m_term.connect_to_server(container_pid);
}

void Container::manage_container(const std::string& path, const std::string& filesystem_dir) {
    if (fork() != 0) {
        exit(0);
    }
    if (setsid() == -1) {
        Utils::handle_error("setsid failed for manager");
    }

    int parent_to_child_pipe[2];
    int child_to_parent_pipe[2];
    if (pipe(parent_to_child_pipe) == ERR || pipe(child_to_parent_pipe) == ERR) {
        Utils::handle_error("pipe creation failed");
    }

    m_child_pid = fork();
    if (m_child_pid == -1) {
        Utils::handle_error("fork failed");
    }

    if (m_child_pid == 0) {
        close(parent_to_child_pipe[1]);
        close(child_to_parent_pipe[0]);

        if (unshare(CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNET | CLONE_NEWNS) != 0) {
            Utils::handle_error("unshare failed");
        }

        char ready_signal{ '1' };
        if (write(child_to_parent_pipe[1], &ready_signal, 1) != 1) {
            Utils::handle_error("could not signal parent");
        }
        close(child_to_parent_pipe[1]);

        char sync_signal{};
        if (read(parent_to_child_pipe[0], &sync_signal, 1) != 1) {
            Utils::handle_error("Failed to receive sync from parent");
        }
        close(parent_to_child_pipe[0]);

        pid_t container_init { fork() };
        if (container_init == ERR) {
            Utils::handle_error("fork for PID namespace failed");
        }

        if (container_init == 0) {
            ContainerArgs args{ m_new_hostname, m_new_fs, path, m_pty_args.slave_fd, filesystem_dir, m_commands };
            run_container(args);
        } else {
            close(m_pty_args.slave_fd);

            int status{};
            waitpid(container_init, &status, 0);
            exit(WIFEXITED(status) ? WEXITSTATUS(status) : 1);
        }

    } else {
        close(parent_to_child_pipe[0]);
        close(child_to_parent_pipe[1]);

        char ready_signal{};
        if (read(child_to_parent_pipe[0], &ready_signal, 1) != 1) {
            Utils::handle_error("could not read signal from child");
        }
        close(child_to_parent_pipe[0]);

        setup_user_namespace();

        char go_signal{ '1' };
        if (write(parent_to_child_pipe[1], &go_signal, 1) != 1) {
            Utils::handle_error("Failed to write sync to child");
        }
        close(parent_to_child_pipe[1]);

        if (Network::setup_networking(m_child_pid) != 0) {
            Utils::handle_error("Failed to setup network");
        }

        m_db->update_container_pid(m_container_id, m_child_pid);
        m_db->update_container_status(m_container_id, "running");

        m_term.start_server(m_pty_args, m_child_pid, getpid());

        m_db->update_container_status(m_container_id, "exited");

        int status{};
        waitpid(m_child_pid, &status, 0);
        exit(0);
    }
}

void Container::run(const std::string& path, const std::string& container_id) {
    ioctl(STDIN_FILENO, TIOCGWINSZ, &m_pty_args.window_size);
    m_term.start_pty_session(m_pty_args);

    pid_t temp_pid{ getpid() };
    std::string filesystem_dir{ Utils::get_filesystem_path(temp_pid) };
    std::string upper { filesystem_dir + "/upper" };
    std::string merged { filesystem_dir + "/merged" };
    std::string work  { filesystem_dir + "/work" };

    Utils::ensure_dirs(upper);
    Utils::ensure_dirs(work);
    Utils::ensure_dirs(merged);

    std::cerr << "Preparing container filesystem directories..." << '\n';

    pid_t manager_pid{ fork() };
    if (manager_pid == ERR) {
        Utils::handle_error("fork for manager failed");
    }

    if (manager_pid == 0) {
        manage_container(path, filesystem_dir);
    } else {
        // Close both FDs in the parent process
        close(m_pty_args.master_fd);
        close(m_pty_args.slave_fd);

        std::cerr << "Container started." << '\n';
        std::cerr << "To attach, run: quiver attach " << manager_pid << '\n';

        int status;
        waitpid(manager_pid, &status, 0);
    }
}

void Container::run_container(const ContainerArgs& args) {
    if (setsid() == ERR) {
        Utils::handle_error("setsid error");
    }
    m_term.redirect_io(args.slave_fd);
    close(args.slave_fd);

    if (sethostname(args.hostname.c_str(), args.hostname.size()) == ERR)
        Utils::handle_error("Unable to set hostname of container");

    std::string filesystem_path{ args.filesystem_dir };
    std::string merged { filesystem_path + "/merged" };
    std::string upper { filesystem_path + "/upper" };
    std::string work { filesystem_path + "/work" };

    std::cerr << "DEBUG: Container init PID: " << getpid() << '\n';

    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == ERR) {
        std::cerr << "WARNING: Could not make / private: " << strerror(errno) << '\n';
    }

    std::cerr << "DEBUG: Mounting overlayfs..." << '\n';
    std::string overlay_options { "lowerdir=" + args.rootfs_path +
                                  ",upperdir=" + upper +
                                  ",workdir=" + work };

    if (mount("overlay", merged.c_str(), "overlay", MS_NODEV, overlay_options.c_str()) == ERR) {
        std::cerr << "ERROR: Failed to mount overlayfs: " << strerror(errno) << '\n';
        std::cerr << "DEBUG: Options were: " << overlay_options << '\n';
        Utils::handle_error("Cannot mount overlay filesystem");
    }

    std::cerr << "DEBUG: Overlay mounted successfully at: " << merged << '\n';
    std::cerr << "DEBUG: Setting up merged as mount point..." << '\n';

    if (mount(merged.c_str(), merged.c_str(), NULL, MS_BIND | MS_REC, NULL) == ERR) {
        Utils::handle_error("Unable to bind mount merged");
    }

    if (mount(NULL, merged.c_str(), NULL, MS_PRIVATE | MS_REC, NULL) == ERR) {
        Utils::handle_error("Unable to make merged private");
    }

    if (chdir(merged.c_str()) == ERR) {
        Utils::handle_error("Unable to change directory to " + merged);
    }

    Mount::volumes(merged,m_volumes);
    std::cerr << "DEBUG: Creating old_root..." << '\n';
    Utils::ensure_dirs("old_root");
    std::cerr << "DEBUG: Performing pivot_root..." << '\n';

    if (syscall(SYS_pivot_root, ".", "old_root") == ERR) {
        std::cerr << "ERROR: pivot_root failed, errno=" << errno
                  << " (" << strerror(errno) << ")" << '\n';
        Utils::handle_error("Unable to pivot root");
    }

    std::cerr << "DEBUG: pivot_root successful!" << '\n';
    if (chdir("/") == ERR)
        Utils::handle_error("Unable to change dir to /");

    std::cerr << "DEBUG: Creating mount point directories..." << '\n';
    std::string proc { "/proc" };
    std::string sys { "/sys" };
    std::string dev { "/dev" };
    std::string etc { "/etc" };

    Utils::ensure_dirs(proc);
    Utils::ensure_dirs(sys);
    Utils::ensure_dirs(dev);
    Utils::ensure_dirs(etc);
    std::cerr << "DEBUG: Mounting special filesystems..." << '\n';

    Mount::proc(proc, MS_NODEV | MS_NOEXEC | MS_NOSUID);
    Mount::sys(sys, MS_NODEV | MS_NOSUID | MS_NOEXEC);
    Mount::tmpfs(dev, 0, "mode=0755");

    std::cerr << "DEBUG: Setting up /dev devices from old_root..." << '\n';
    DeviceManager::create_terminal_devices();

    std::cerr << "DEBUG: Unmounting old_root..." << '\n';
    umount2("/old_root", MNT_DETACH);
    rmdir("/old_root");

    std::cout << "Container setup successful!" << '\n';

    std::ofstream resolv("/etc/resolv.conf");
    if (resolv.is_open()) {
        resolv << "nameserver 10.0.2.3\n";
        resolv.close();
    }

    if(PackageManager::initialize() == ERR) {
        std::cerr << "Warning: Package manager initialization failed, but continuing..." << '\n';
    }

    clearenv();
    setenv("TERM", "xterm", 0);
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);

    if (!args.commands.empty()) {
        std::cerr << "DEBUG: Executing " << args.commands.size() << " commands" << '\n';

        const char* shell{ nullptr };
        const char* shells[]{ "/bin/bash", "/bin/sh", "/bin/ash", nullptr };
        struct stat st;

        for (int i = 0; shells[i] != nullptr; i++) {
            if(Utils::path_exists(shell)){
                shell = shells[i];
            }
        }

        if (shell == nullptr) {
            Utils::handle_error("No shell found to execute commands");
        }

        std::string cmd_string{};
        for (size_t i = 0; i < args.commands.size(); i++) {
            cmd_string += args.commands[i];
            if (i < args.commands.size() - 1) {
                cmd_string += " && ";
            }
        }

        std::cerr << "DEBUG: Executing: " << cmd_string << '\n';

        execl(shell, shell, "-c", cmd_string.c_str(), (char*)nullptr);

        std::cerr << "ERROR: execl failed, errno=" << errno << " (" << strerror(errno) << ")" << '\n';
        Utils::handle_error("Failed to execute commands");
    } else {
        std::cerr << "DEBUG: Executing " << args.program_path << '\n';
        execl(args.program_path.c_str(), args.program_path.c_str(), (char*)nullptr);

        std::cerr << "ERROR: execl failed, errno=" << errno
                  << " (" << strerror(errno) << ")" << '\n';
        Utils::handle_error("Failed to execute " + args.program_path);
    }
}

void Container::setup_user_namespace() {
    uid_t host_uid{ getuid() };
    gid_t host_gid{ getgid() };

    std::string uid_map{ "0 " + std::to_string(host_uid) + " 1\n" };
    std::string gid_map{ "0 " + std::to_string(host_gid) + " 1\n" };

    std::string uid_map_path{ "/proc/" + std::to_string(m_child_pid) + "/uid_map" };
    std::string gid_map_path{ "/proc/" + std::to_string(m_child_pid) + "/gid_map" };
    std::string setgroups_path{ "/proc/" + std::to_string(m_child_pid) + "/setgroups" };

    Utils::write_file(setgroups_path, "deny\n");
    Utils::write_file(gid_map_path, gid_map);
    Utils::write_file(uid_map_path, uid_map);
}
