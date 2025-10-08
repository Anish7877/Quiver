#include "../include/container.hpp"
#include "../include/utils.hpp"
#include "../include/network.hpp"
#include "../include/package_manager.hpp"
#include "../include/mount.hpp"
#include "../include/device_manager.hpp"
#include "../include/fuse_overlay.hpp"
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
Terminal Container::m_term{};
Terminal::PtyArgs Container::m_pty_args{};

Container::Container(const std::string& hostname, const std::string& new_fs, const std::vector<std::string>& volumes){
    m_new_hostname = hostname;
    m_new_fs = new_fs;
    m_volumes = volumes;
}

void Container::exec(const std::string& program_path){
    run(program_path);
}

void Container::set_filesystem(const std::string& path){
    m_new_fs = path;
}

void Container::connect_to_server(const pid_t& container_pid){
    m_term.connect_to_server(container_pid);
}

// NEW: This function runs in a detached process to manage the container and server
void Container::manage_container(const std::string& path, const std::string& filesystem_dir) {
    // Daemonize the manager process
    if (fork() != 0) {
        exit(0);
    }
    if (setsid() == -1) {
        Utils::handle_error("setsid failed for manager");
    }

    int parent_to_child_pipe[2];
    int child_to_parent_pipe[2];
    if (pipe(parent_to_child_pipe) == -1 || pipe(child_to_parent_pipe) == -1) {
        Utils::handle_error("pipe creation failed");
    }

    m_child_pid = fork();
    if (m_child_pid == -1) {
        Utils::handle_error("fork failed");
    }

    if (m_child_pid == 0) { // Child process: becomes the container
        close(parent_to_child_pipe[1]);
        close(child_to_parent_pipe[0]);

        if (unshare(CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNET | CLONE_NEWNS) != 0) {
            Utils::handle_error("unshare failed");
        }

        char ready_signal = '1';
        if (write(child_to_parent_pipe[1], &ready_signal, 1) != 1) {
            Utils::handle_error("could not signal parent");
        }
        close(child_to_parent_pipe[1]);

        char sync_signal;
        if (read(parent_to_child_pipe[0], &sync_signal, 1) != 1) {
            Utils::handle_error("Failed to receive sync from parent");
        }
        close(parent_to_child_pipe[0]);

        pid_t container_init { fork() };
        if (container_init == -1) {
            Utils::handle_error("fork for PID namespace failed");
        }

        if (container_init == 0) {
            ContainerArgs args{ m_new_hostname, m_new_fs, path, m_pty_args.slave_fd, filesystem_dir };
            run_container(args);
        } else {
            int status{};
            waitpid(container_init, &status, 0);
            exit(WIFEXITED(status) ? WEXITSTATUS(status) : 1);
        }

    } else { // Parent process: the container manager/server
        close(parent_to_child_pipe[0]);
        close(child_to_parent_pipe[1]);

        char ready_signal{};
        if (read(child_to_parent_pipe[0], &ready_signal, 1) != 1) {
            Utils::handle_error("could not read signal from child");
        }
        close(child_to_parent_pipe[0]);

        setup_user_namespace();

        char go_signal = '1';
        if (write(parent_to_child_pipe[1], &go_signal, 1) != 1) {
            Utils::handle_error("Failed to write sync to child");
        }
        close(parent_to_child_pipe[1]);

        if (Network::setup_networking(m_child_pid) != 0) {
            Utils::handle_error("Failed to setup network");
        }

        // The manager process now starts the terminal server.
        // The server's socket will be identified by the manager's PID.
        m_term.start_server(m_pty_args, m_child_pid, getpid());

        // When start_server returns, the container has exited.
        int status;
        waitpid(m_child_pid, &status, 0);
        exit(0);
    }
}

void Container::run(const std::string& path) {
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

    std::cerr << "Setting up FUSE overlay before fork..." << '\n';
    FuseOverlay::setup(m_new_fs, upper, work, merged);
    std::cerr << "FUSE overlay ready at: " << merged << '\n';

    pid_t manager_pid = fork();
    if (manager_pid == -1) {
        Utils::handle_error("fork for manager failed");
    }

    if (manager_pid == 0) {
        // This child process becomes the detached container manager
        manage_container(path, filesystem_dir);
    } else {
        // The original process prints the manager's PID for the user and exits.
        std::cerr << "Container started." << '\n';
        std::cerr << "To attach, run: quiver attach " << manager_pid << '\n';
        // Wait for the first fork in manage_container to exit, ensuring daemonization has started.
        waitpid(manager_pid, NULL, 0);
    }
}

void Container::run_container(const ContainerArgs& args) {
    if (setsid() == ERR) {
        Utils::handle_error("setsid error");
    }
    m_term.redirect_io(args.slave_fd);

    if (sethostname(args.hostname.c_str(), args.hostname.size()) == ERR)
        Utils::handle_error("Unable to set hostname of container");

    std::string filesystem_path{ args.filesystem_dir };
    std::string merged { filesystem_path + "/merged" };
    std::string upper { filesystem_path + "/upper" };

    std::cerr << "DEBUG: Container init PID: " << getpid() << '\n';
    std::cerr << "DEBUG: Working with merged: " << merged << '\n';

    // Make the entire root filesystem private first
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) == ERR) {
        std::cerr << "WARNING: Could not make / private: " << strerror(errno) << '\n';
    }

    // Create directories in upper layer
    std::string upper_proc { upper + "/proc" };
    std::string upper_sys { upper + "/sys" };
    std::string upper_dev { upper + "/dev" };
    std::string upper_etc { upper + "/etc" };
    std::string upper_old { upper + "/old_root" };

    Utils::ensure_dirs(upper_proc);
    Utils::ensure_dirs(upper_sys);
    Utils::ensure_dirs(upper_dev);
    Utils::ensure_dirs(upper_etc);
    Utils::ensure_dirs(upper_old);

    // Make merged a mount point
    std::cerr << "DEBUG: Setting up merged as mount point..." << '\n';
    if (mount(merged.c_str(), merged.c_str(), NULL, MS_BIND | MS_REC, NULL) == ERR) {
        Utils::handle_error("Unable to bind mount merged");
    }

    if (mount(NULL, merged.c_str(), NULL, MS_PRIVATE | MS_REC, NULL) == ERR) {
        Utils::handle_error("Unable to make merged private");
    }

    // Change to merged directory
    if (chdir(merged.c_str()) == ERR) {
        Utils::handle_error("Unable to change directory to " + merged);
    }

    // Create old_root
    std::cerr << "DEBUG: Creating old_root..." << '\n';
    if (mkdir("old_root", 0755) == ERR && errno != EEXIST) {
        Utils::handle_error("Unable to create old_root directory");
    }

    std::cerr << "DEBUG: Performing pivot_root..." << '\n';
    if (syscall(SYS_pivot_root, ".", "old_root") == ERR) {
        std::cerr << "ERROR: pivot_root failed, errno=" << errno
                  << " (" << strerror(errno) << ")" << '\n';
        Utils::handle_error("Unable to pivot root");
    }

    std::cerr << "DEBUG: pivot_root successful!" << '\n';
    if (chdir("/") == ERR)
        Utils::handle_error("Unable to change dir to /");

    // Create mount point directories in new root
    std::cerr << "DEBUG: Creating mount point directories..." << '\n';
    std::string proc { "/proc" };
    std::string sys { "/sys" };
    std::string dev { "/dev" };
    std::string etc { "/etc" };

    if (mkdir(proc.c_str(), 0755) == ERR && errno != EEXIST) {
        Utils::handle_error("Unable to create /proc directory");
    }
    if (mkdir(sys.c_str(), 0755) == ERR && errno != EEXIST) {
        Utils::handle_error("Unable to create /sys directory");
    }
    if (mkdir(dev.c_str(), 0755) == ERR && errno != EEXIST) {
        Utils::handle_error("Unable to create /dev directory");
    }
    if (mkdir(etc.c_str(), 0755) == ERR && errno != EEXIST) {
        Utils::handle_error("Unable to create /etc directory");
    }

    std::cerr << "DEBUG: Mounting special filesystems..." << '\n';

    // Mount special filesystems - now we're truly PID 1 in the namespace
    Mount::proc(proc, MS_NODEV | MS_NOEXEC | MS_NOSUID);
    Mount::sys(sys, MS_NODEV | MS_NOSUID | MS_NOEXEC);
    Mount::tmpfs(dev, 0, "mode=0755");

    std::cerr << "DEBUG: Setting up /dev devices from old_root..." << '\n';

    // Bind mount essential devices from old root (before we unmount it)
    struct DeviceBind {
        const char* name;
        bool required;
    };

    DeviceBind devices[] = {
        {"null", true},
        {"zero", true},
        {"full", true},
        {"random", true},
        {"urandom", true},
        {"tty", true},
        {"console", false},
    };

    for (const auto& device : devices) {
        std::string old_path = std::string("/old_root/dev/") + device.name;
        std::string new_path = std::string("/dev/") + device.name;

        // Check if source exists in old root
        struct stat st;
        if (stat(old_path.c_str(), &st) != 0) {
            if (device.required) {
                std::cerr << "WARNING: " << old_path << " not found in old root" << '\n';
            }
            continue;
        }

        // Create a regular file as mount target
        int fd = open(new_path.c_str(), O_CREAT | O_RDONLY, 0666);
        if (fd >= 0) close(fd);

        // Bind mount the device
        if (mount(old_path.c_str(), new_path.c_str(), nullptr, MS_BIND, nullptr) == 0) {
            std::cerr << "DEBUG: Bind mounted " << old_path << " to " << new_path << '\n';
        } else {
            std::cerr << "WARNING: Failed to bind mount " << device.name << ": "
                      << strerror(errno) << '\n';
        }
    }

    // Create /dev/pts
    std::cerr << "DEBUG: Setting up /dev/pts..." << '\n';
    if (mkdir("/dev/pts", 0755) == -1 && errno != EEXIST) {
        std::cerr << "WARNING: mkdir /dev/pts failed: " << strerror(errno) << '\n';
    }

    // Mount devpts
    int devpts_flags = MS_NOSUID | MS_NOEXEC;
    std::string devpts_options = "newinstance,ptmxmode=0666";
    if (mount("devpts", "/dev/pts", "devpts", devpts_flags, devpts_options.c_str()) == -1) {
        std::cerr << "WARNING: devpts mount failed, trying simpler options: " << strerror(errno) << '\n';
        // Try without newinstance
        if (mount("devpts", "/dev/pts", "devpts", devpts_flags, "ptmxmode=0666") == -1) {
            std::cerr << "WARNING: devpts mount still failed: " << strerror(errno) << '\n';
        }
    }

    // Create /dev/ptmx symlink or bind mount
    unlink("/dev/ptmx");
    if (symlink("pts/ptmx", "/dev/ptmx") == -1) {
        std::cerr << "DEBUG: symlink /dev/ptmx failed, trying bind mount: " << strerror(errno) << '\n';
        // Try bind mounting from old root
        int fd = open("/dev/ptmx", O_CREAT | O_RDONLY, 0666);
        if (fd >= 0) close(fd);
        if (mount("/old_root/dev/ptmx", "/dev/ptmx", nullptr, MS_BIND, nullptr) == -1) {
            std::cerr << "WARNING: bind mount /dev/ptmx failed: " << strerror(errno) << '\n';
        }
    }

    // Create standard fd symlinks
    symlink("/proc/self/fd", "/dev/fd");
    symlink("/proc/self/fd/0", "/dev/stdin");
    symlink("/proc/self/fd/1", "/dev/stdout");
    symlink("/proc/self/fd/2", "/dev/stderr");

    // Create /dev/shm for shared memory
    if (mkdir("/dev/shm", 0755) == -1 && errno != EEXIST) {
        std::cerr << "WARNING: mkdir /dev/shm failed: " << strerror(errno) << '\n';
    }
    if (mount("shm", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") == -1) {
        std::cerr << "WARNING: mount /dev/shm failed: " << strerror(errno) << '\n';
    }

    std::cerr << "DEBUG: Device setup complete" << '\n';

    // Unmount old root
    std::cerr << "DEBUG: Unmounting old_root..." << '\n';
    umount2("/old_root", MNT_DETACH);
    rmdir("/old_root");

    std::cout << "Container setup successful!" << '\n';

    // Setup network configuration
    std::ofstream resolv("/etc/resolv.conf");
    if (resolv.is_open()) {
        resolv << "nameserver 10.0.2.3\n";
        resolv.close();
    }

    if(PackageManager::initialize() == ERR)
        Utils::handle_error("cannot setup package manager");

    std::cerr << "DEBUG: Executing " << args.program_path << '\n';
    execl(args.program_path.c_str(), args.program_path.c_str(), (char*)NULL);

    std::cerr << "ERROR: execl failed, errno=" << errno
              << " (" << strerror(errno) << ")" << '\n';
    Utils::handle_error("Failed to execute " + args.program_path);
}

void Container::setup_user_namespace() {
    // Get the real UID and GID of the user running this program on the host
    uid_t host_uid = getuid();
    gid_t host_gid = getgid();

    // --- START OF THE FIX ---
    // Construct the mapping strings dynamically using the real host UID/GID.
    // This maps UID 0 inside the container to the host's current user, and the same for the GID.
    std::string uid_map = "0 " + std::to_string(host_uid) + " 1\n";
    std::string gid_map = "0 " + std::to_string(host_gid) + " 1\n";
    // --- END OF THE FIX ---

    std::string uid_map_path = "/proc/" + std::to_string(m_child_pid) + "/uid_map";
    std::string gid_map_path = "/proc/" + std::to_string(m_child_pid) + "/gid_map";
    std::string setgroups_path = "/proc/" + std::to_string(m_child_pid) + "/setgroups";

    // This order is critical and correct.
    // 1. Deny setgroups
    Utils::write_file(setgroups_path, "deny\n");
    // 2. Map GID
    Utils::write_file(gid_map_path, gid_map);
    // 3. Map UID
    Utils::write_file(uid_map_path, uid_map);
}
