#include "../include/process.hpp"
#include "../include/tty_proxy_server.hpp"

// static variables
pid_t Process::m_child_pid{-1};
std::string Process::m_new_hostname{"container"};
std::string Process::m_new_fs{"/home/anish/NEW_ROOT"};

int Process::start(const std::string& new_hostname,const std::string& filesystem_path,std::string_view path){
    m_new_hostname = new_hostname;
    m_new_fs = filesystem_path;
    if(run(path) == -1) return -1;
    return 0;
}
int Process::handle_error(std::string_view err){
    std::cerr << err << '\n';
    return -1;
}

int Process::run(std::string_view path){
    int flags{ CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWNS };
    if(unshare(flags) != 0) return handle_error("Namespace creation error");

    if(write_file("/proc/self/uid_map", "0 1000 1\n") == -1) return -1;
    if(write_file("/proc/self/setgroups", "deny\n") == -1) return -1;
    if(write_file("/proc/self/gid_map", "0 1000 1\n") == -1) return -1;

    int master_fd { -1 }, slave_fd { -1 };
    char slave_name[128] = {0};
    winsize ws = {24, 80, 0, 0};
    if (openpty(&master_fd, &slave_fd, slave_name, NULL, &ws) == -1) {
        return handle_error("openpty failed");
    }

    m_child_pid = fork();
    if(m_child_pid == -1) return handle_error("fork error");

    if(m_child_pid == 0){
        close(master_fd);

        if(unshare(CLONE_NEWPID) != 0) return handle_error("PID namespace creation error");

        pid_t grandchild { fork() };
        if(grandchild == -1) return handle_error("second fork error");

        if(grandchild == 0) {
            if (setsid() == -1) {
            }
            if (ioctl(slave_fd, TIOCSCTTY, 0) == -1) {
            }

            if (dup2(slave_fd, STDIN_FILENO) == -1) return handle_error("dup2 stdin failed");
            if (dup2(slave_fd, STDOUT_FILENO) == -1) return handle_error("dup2 stdout failed");
            if (dup2(slave_fd, STDERR_FILENO) == -1) return handle_error("dup2 stderr failed");
            if (slave_fd > STDERR_FILENO) close(slave_fd);

            if(sethostname(m_new_hostname.c_str(),m_new_hostname.length()) < 0)
                return handle_error("Unable to change hostname");

            mkdir((m_new_fs + "/proc").c_str(), 0755);
            mkdir((m_new_fs + "/sys").c_str(), 0755);
            mkdir((m_new_fs + "/dev").c_str(), 0755);
            mkdir((m_new_fs + "/old_root").c_str(), 0755);

            std::string new_proc_path = m_new_fs + "/proc";
            std::cerr << "DEBUG: Mounting proc to " << new_proc_path << " before pivot..." << std::endl;
            if(mount("proc", new_proc_path.c_str(), "proc", 0, NULL) == -1) {
                std::cerr << "DEBUG: Pre-pivot proc mount failed: " << strerror(errno) << std::endl;
            } else {
                std::cerr << "DEBUG: Pre-pivot proc mount successful!" << std::endl;
            }

            std::string new_sys_path = m_new_fs + "/sys";
            std::string new_dev_path = m_new_fs + "/dev";

            if(mount("sysfs", new_sys_path.c_str(), "sysfs", MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) == -1) {
                std::cerr << "DEBUG: Pre-pivot sys mount failed: " << strerror(errno) << std::endl;
            } else {
                std::cerr << "DEBUG: Pre-pivot sys mount successful!" << std::endl;
            }

            if(mount("tmpfs", new_dev_path.c_str(), "tmpfs", 0, NULL) == -1) {
                std::cerr << "DEBUG: Pre-pivot dev mount failed: " << strerror(errno) << std::endl;
            } else {
                std::cerr << "DEBUG: Pre-pivot dev mount successful!" << std::endl;
            }

            if(mount(m_new_fs.c_str(), m_new_fs.c_str(), nullptr, MS_BIND | MS_REC, NULL) == -1)
                return handle_error("Unable to mount new fs");

            if(pivot_root() == -1)
                return handle_error("Unable to pivot the root of the old fs");

            if(chdir("/") == -1)
                return handle_error("Unable to change directory to /");

            if(umount2("/old_root", MNT_DETACH) == -1)
                return handle_error("Unable to unmount old root");

            rmdir("/old_root");

            std::cerr << "DEBUG: Pivot completed successfully!" << std::endl;

            std::ifstream mounts("/proc/mounts");
            if(mounts.is_open()) {
                std::cerr << "DEBUG: /proc/mounts is accessible, proc is mounted!" << std::endl;
                mounts.close();
            } else {
                std::cerr << "DEBUG: /proc/mounts not accessible, proc may not be mounted" << std::endl;

                if(mount("proc", "/proc", "proc", MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) == -1) {
                    std::cerr << "DEBUG: Post-pivot proc mount failed: " << strerror(errno) << std::endl;
                    return handle_error("Unable to mount /proc of new fs");
                }
            }

            execlp(path.data(), path.data(), (char*) NULL);
            return handle_error("exec failed");
        } else {
            waitpid(grandchild, NULL, 0);
            exit(0);
        }
    } else {
        close(slave_fd);

        pid_t proxy_pid { fork() };
        if (proxy_pid == -1) {
            close(master_fd);
            return handle_error("fork for proxy failed");
        }
        TTYProxyServer tty{};
        if (proxy_pid == 0) {
            std::string sock{ tty.get_sock_path(m_child_pid) };
            std::cerr << "DEBUG: tty proxy socket: " << sock << std::endl;
            if(tty.start(master_fd,m_child_pid,sock) == -1)
                handle_error("Proxy server start failed");
            _exit(1);
        } else {
            close(master_fd);
            std::string sock{ tty.get_sock_path(m_child_pid) };
            std::cerr << "Container started. attach socket: " << sock << "\n";
            return 0;
        }
    }
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
