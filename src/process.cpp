#include "../include/process.hpp"
#include "../include/tty_proxy_server.hpp"
#include "../include/network.hpp"
#include <cstdlib>

// static variables
pid_t Process::m_child_pid{-1};
std::string Process::m_new_hostname{""};
std::string Process::m_new_fs{""};

// Stack for clone
static constexpr size_t STACK_SIZE = 1024 * 1024; // 1MB stack
static char child_stack[STACK_SIZE];


int Process::start(const std::string& new_hostname,const std::string& filesystem_path,const std::string& path){
    m_new_hostname = new_hostname;
    m_new_fs = filesystem_path;
    if(run(path) == -1) return -1;
    return 0;
}

int Process::handle_error(const std::string& err){
    std::cerr << err << '\n';
    return -1;
}

std::string Process::get_filesystem_dir(pid_t pid){
    std::string home { getenv("HOME") };
    std::string base { home + "/.quiver/filesystems/" };
    std::string dir { base + std::to_string(long(pid)) + "/"};
    return dir;
}

void Process::ensure_dirs(const std::string& dir){
    size_t start { 1 };
    size_t pos { 0 };
    while(true){
        pos = dir.find("/",start);
        std::string prefix { pos != std::string::npos ? dir.substr(0,pos) : dir };
        if(!prefix.empty()){
            if(mkdir(prefix.c_str(),0755) == -1){
            }
        }
        if(pos == std::string::npos){
            break;
        }
        start = pos+1;
    }
}

// Static wrapper function for clone
static int container_main(void* arg) {
    ContainerArgs* args = static_cast<ContainerArgs*>(arg);
    return Process::run_container(args);
}

int Process::run_container(ContainerArgs* args) {
    // Setup TTY
    if (setsid() == -1) {
        std::cerr << "setsid failed" << '\n';
    }

    if (ioctl(args->slave_fd, TIOCSCTTY, 0) == -1) {
        std::cerr << "TIOCSCTTY failed" << '\n';
    }

    // Redirect stdio
    dup2(args->slave_fd, STDIN_FILENO);
    dup2(args->slave_fd, STDOUT_FILENO);
    dup2(args->slave_fd, STDERR_FILENO);
    if (args->slave_fd > STDERR_FILENO) close(args->slave_fd);

    // Set hostname
    if (sethostname(args->hostname.c_str(), args->hostname.length()) != 0) {
        std::cerr << "Failed to set hostname: " << strerror(errno) << '\n';
        return -1;
    }

    // Setup filesystem using your existing overlay approach
    std::string filesystem_path = args->filesystem_dir;
    std::string upper { filesystem_path + "upper" };
    std::string work  { filesystem_path + "work" };
    std::string merged { filesystem_path + "merged" };

    Process::ensure_dirs(upper);
    Process::ensure_dirs(work);
    Process::ensure_dirs(merged);

    pid_t overlay_pid { fork() };
    if (overlay_pid == 0) {
        const char *fuse_bin { "/usr/bin/fuse-overlayfs" };
        execlp(fuse_bin, fuse_bin,
                "-o", ("lowerdir=" + args->rootfs_path).c_str(),
                "-o", ("upperdir=" + upper).c_str(),
                "-o", ("workdir=" + work).c_str(),
                merged.c_str(),
                (char*) NULL);
        _exit(1);
    }
    waitpid(overlay_pid, NULL, 0);

    // Create necessary directories in new root
    mkdir((merged + "/proc").c_str(), 0755);
    mkdir((merged + "/sys").c_str(), 0755);
    mkdir((merged + "/dev").c_str(), 0755);
    mkdir((merged + "/old_root").c_str(), 0755);
    mkdir((merged + "/etc").c_str(), 0755);

    // Pre-mount essential filesystems
    std::string new_proc_path = merged + "/proc";
    std::string new_sys_path  = merged + "/sys";
    std::string new_dev_path  = merged + "/dev";

    mount("proc", new_proc_path.c_str(), "proc", MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL);
    mount("sysfs", new_sys_path.c_str(), "sysfs", MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL);
    mount("tmpfs", new_dev_path.c_str(), "tmpfs", 0, NULL);

    if(mount(merged.c_str(), merged.c_str(), nullptr, MS_BIND | MS_REC, NULL) == -1) {
        std::cerr << "Unable to mount new fs: " << strerror(errno) << '\n';
        return -1;
    }

    // Change to merged directory and pivot root
    if (chdir(merged.c_str()) != 0) {
        std::cerr << "chdir to merged failed: " << strerror(errno) << '\n';
        return -1;
    }

    if (syscall(SYS_pivot_root, ".", "old_root") != 0) {
        std::cerr << "pivot_root failed: " << strerror(errno) << '\n';
        return -1;
    }

    if (chdir("/") != 0) {
        std::cerr << "chdir to / failed: " << strerror(errno) << '\n';
        return -1;
    }

    // Unmount old root
    umount2("/old_root", MNT_DETACH);
    rmdir("/old_root");

    std::cerr << "DEBUG: Pivot completed successfully!" << '\n';
    // In run_container(), after pivot_root but before exec
    // Create /etc/resolv.conf
    std::ofstream resolv("/etc/resolv.conf");
    resolv << "nameserver 10.0.2.3\n";  // slirp4netns default DNS
    resolv.close();
    // Configure network interface inside container
    std::cerr << "Starting program: " << args->program_path << '\n';
    execl(args->program_path.c_str(), args->program_path.c_str(), (char*)NULL);

    std::cerr << "exec failed: " << strerror(errno) << '\n';
    return -1;
}

int Process::run(const std::string& path){
    // Create PTY pair
    int master_fd, slave_fd;
    char slave_name[128];
    winsize ws{};
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);

    if (openpty(&master_fd, &slave_fd, slave_name, NULL, &ws) == -1) {
        return handle_error("openpty failed");
    }

    // Prepare arguments for container
    std::string filesystem_dir = get_filesystem_dir(getpid());
    ContainerArgs args{m_new_hostname, m_new_fs, path, slave_fd, filesystem_dir};

    // Clone with all namespaces
    int clone_flags = CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWNS |
                     CLONE_NEWPID | CLONE_NEWNET | SIGCHLD;

    m_child_pid = clone(container_main,
                       child_stack + STACK_SIZE,
                       clone_flags,
                       &args);

    if (m_child_pid == -1) {
        close(master_fd);
        close(slave_fd);
        return handle_error("clone failed");
    }

    close(slave_fd);

    std::cerr << "Container started with PID: " << m_child_pid << '\n';

    // Setup user namespace mappings
    if (setup_user_namespace() != 0) {
        std::cerr << "Failed to setup user namespace" << '\n';
        return -1;
    }

    if (Network::setup_networking(m_child_pid) != 0) {
        std::cerr << "Failed to setup networking" << '\n';
        return -1;
    }

    pid_t proxy_pid = fork();
    if (proxy_pid == -1) {
        close(master_fd);
        return handle_error("fork for proxy failed");
    }

    TTYProxyServer tty{};
    if (proxy_pid == 0) {
        std::string sock = tty.get_sock_path(m_child_pid);
        std::cerr << "DEBUG: tty proxy socket: " << sock << '\n';
        if(tty.start(master_fd, m_child_pid, sock) == -1)
            handle_error("Proxy server start failed");
        _exit(1);
    } else {
        close(master_fd);

        // Wait for container
        int status;
        waitpid(m_child_pid, &status, WNOHANG);

        std::string sock { tty.get_sock_path(m_child_pid) };
        std::cerr << "Container started. attach socket: " << sock << '\n';
        return 0;
    }
}

int Process::setup_user_namespace() {
    std::string uid_map = "/proc/" + std::to_string(m_child_pid) + "/uid_map";
    std::string gid_map = "/proc/" + std::to_string(m_child_pid) + "/gid_map";
    std::string setgroups = "/proc/" + std::to_string(m_child_pid) + "/setgroups";

    // Write setgroups
    if (write_file(setgroups, "deny\n") == -1) return -1;

    // Write uid_map
    std::string uid_mapping = "0 " + std::to_string(getuid()) + " 1\n";
    if (write_file(uid_map, uid_mapping) == -1) return -1;

    // Write gid_map
    std::string gid_mapping = "0 " + std::to_string(getgid()) + " 1\n";
    if (write_file(gid_map, gid_mapping) == -1) return -1;

    return 0;
}

int Process::write_file(const std::string& path,const std::string& str){
    int fd{ open(path.c_str() , O_WRONLY) };
    if(fd == -1){
        std::cerr << "Bad File Descriptor for " << path << '\n';
        return -1;
    }
    if(write(fd, str.c_str(), str.length()) == -1){
        std::cerr << "Unable to Write to a " << path << '\n';
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

int Process::pivot_root(){
    return syscall(SYS_pivot_root,m_new_fs.c_str(),(m_new_fs+"/old_root").c_str());
}

Process::~Process(){
}
