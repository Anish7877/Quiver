#include "../include/process.hpp"
#include "../include/tty_proxy_server.hpp"

// static variables
pid_t Process::m_child_pid{-1};
std::string Process::m_new_hostname{""};
std::string Process::m_new_fs{""};

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
int Process::run(std::string_view path){
    int flags{ CLONE_NEWUTS | CLONE_NEWNS };
    if(unshare(CLONE_NEWUSER) != 0) return handle_error("User Namespace creation error");

    if(write_file("/proc/self/uid_map", "0 1000 1\n") == -1) return -1;
    if(write_file("/proc/self/setgroups", "deny\n") == -1) return -1;
    if(write_file("/proc/self/gid_map", "0 1000 1\n") == -1) return -1;

    if(unshare(flags) != 0) return handle_error("Namespace creation error");

    int master_fd { -1 }, slave_fd { -1 };
    char slave_name[128] { 0 };
    winsize ws {};
    ioctl(STDIN_FILENO,TIOCGWINSZ,&ws); // dynamic window size
    if (openpty(&master_fd, &slave_fd, slave_name, NULL, &ws) == -1) {
        return handle_error("openpty failed");
    }

    // PIPE: child -> parent to pass grandchild PID
    int pid_pipe[2]{};
    if (pipe(pid_pipe) == -1) {
        close(master_fd);
        close(slave_fd);
        return handle_error("pipe failed");
    }

    m_child_pid = fork();
    if(m_child_pid == -1) {
        close(master_fd);
        close(slave_fd);
        close(pid_pipe[0]);
        close(pid_pipe[1]);
        return handle_error("fork error");
    }

    if(m_child_pid == 0){
        close(master_fd);

        if(unshare(CLONE_NEWPID) != 0) {
            close(pid_pipe[0]);
            close(pid_pipe[1]);
            return handle_error("PID namespace creation error");
        }

        pid_t grandchild { fork() };
        if(grandchild == -1) {
            close(pid_pipe[0]);
            close(pid_pipe[1]);
            return handle_error("second fork error");
        }

        if(grandchild == 0) {
            close(pid_pipe[0]);
            close(pid_pipe[1]);

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

            std::string filesystem_path { get_filesystem_dir(getpid()) };

            std::string upper { filesystem_path + "upper" };
            std::string work  { filesystem_path + "work" };
            std::string merged { filesystem_path + "merged" };

            ensure_dirs(upper);
            ensure_dirs(work);
            ensure_dirs(merged);

            pid_t overlay_pid { fork() };
            if (overlay_pid == 0) {
                const char *fuse_bin { "/usr/bin/fuse-overlayfs" };
                execlp(fuse_bin, fuse_bin,
                        "-o", ("lowerdir=" + m_new_fs).c_str(),
                        "-o", ("upperdir=" + upper).c_str(),
                        "-o", ("workdir=" + work).c_str(),
                        merged.c_str(),
                        (char*) NULL);
                _exit(1);
            }
            waitpid(overlay_pid, NULL, 0);

            m_new_fs = merged;

            mkdir((m_new_fs + "/proc").c_str(), 0755);
            mkdir((m_new_fs + "/sys").c_str(), 0755);
            mkdir((m_new_fs + "/dev").c_str(), 0755);
            mkdir((m_new_fs + "/old_root").c_str(), 0755);

            std::string new_proc_path = m_new_fs + "/proc";
            std::cerr << "DEBUG: Mounting proc to " << new_proc_path << " before pivot..." << '\n';
            if(mount("proc", new_proc_path.c_str(), "proc", 0, NULL) == -1) {
                std::cerr << "DEBUG: Pre-pivot proc mount failed: " << strerror(errno) << '\n';
            } else {
                std::cerr << "DEBUG: Pre-pivot proc mount successful!" << '\n';
            }

            std::string new_sys_path  { m_new_fs + "/sys" };
            std::string new_dev_path  { m_new_fs + "/dev" };

            if(mount("sysfs", new_sys_path.c_str(), "sysfs", MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) == -1) {
                std::cerr << "DEBUG: Pre-pivot sys mount failed: " << strerror(errno) << '\n';
            } else {
                std::cerr << "DEBUG: Pre-pivot sys mount successful!" << '\n';
            }

            if(mount("tmpfs", new_dev_path.c_str(), "tmpfs", 0, NULL) == -1) {
                std::cerr << "DEBUG: Pre-pivot dev mount failed: " << strerror(errno) << '\n';
            } else {
                std::cerr << "DEBUG: Pre-pivot dev mount successful!" << '\n';
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

            std::cerr << "DEBUG: Pivot completed successfully!" << '\n';

            std::ifstream mounts("/proc/mounts");
            if(mounts.is_open()) {
                std::cerr << "DEBUG: /proc/mounts is accessible, proc is mounted!" << '\n';
                mounts.close();
            } else {
                std::cerr << "DEBUG: /proc/mounts not accessible, proc may not be mounted" << '\n';

                if(mount("proc", "/proc", "proc", MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL) == -1) {
                    std::cerr << "DEBUG: Post-pivot proc mount failed: " << strerror(errno) << '\n';
                    return handle_error("Unable to mount /proc of new fs");
                }
            }

            execl(path.data(), path.data(), (char*) NULL);
            return handle_error("exec failed");
        } else {
            close(pid_pipe[0]); // close read end (child writes)
            char buf[32]{};
            int len { snprintf(buf, sizeof(buf), "%d", (int)grandchild) };
            if (write(pid_pipe[1], buf, (size_t)len) != len) {
                std::cerr << "Warning: failed to write grandchild pid to pipe\n";
            }
            close(pid_pipe[1]);

            // child waits for grandchild to finish, then exit with same status
            int status { 0 };
            waitpid(grandchild, &status, 0);
            if (WIFEXITED(status)) _exit(WEXITSTATUS(status));
            else _exit(1);
        }
    } else {
        close(slave_fd);

        // Close write end of pipe in parent; we'll read grandchild PID
        close(pid_pipe[1]);

        pid_t proxy_pid { fork() };
        if (proxy_pid == -1) {
            close(master_fd);
            close(pid_pipe[0]);
            return handle_error("fork for proxy failed");
        }
        TTYProxyServer tty{};
        if (proxy_pid == 0) {
            std::string sock{ tty.get_sock_path(m_child_pid) };
            std::cerr << "DEBUG: tty proxy socket: " << sock << '\n';
            if(tty.start(master_fd,m_child_pid,sock) == -1)
                handle_error("Proxy server start failed");
            _exit(1);
        } else {
            close(master_fd);

            char rbuf[32] { 0 };
            ssize_t r { read(pid_pipe[0], rbuf, sizeof(rbuf)-1) };
            close(pid_pipe[0]);

            pid_t container_pid { -1 };
            if (r > 0) {
                container_pid = static_cast<pid_t>(atoi(rbuf));
                std::cerr << "DEBUG: received container PID: " << container_pid << '\n';
            } else {
                std::cerr << "DEBUG: didn't receive container PID from child\n";
            }

            // Launch pasta to attach networking to container PID if we got it
            //if (container_pid > 0) {
            //    pid_t pasta_pid { fork() };
            //    if (pasta_pid == 0) {
            //        // child: exec pasta <pid>
            //        std::string pid_str { std::to_string(container_pid) };
            //        const char* pasta_bin { "/usr/bin/pasta" }; // or "pasta" if in PATH
            //        execlp(pasta_bin, pasta_bin, pid_str.c_str(), (char*)NULL);
            //        handle_error("exec pasta failed");
            //    } else if (pasta_pid < 0) {
            //        std::cerr << "Failed to fork pasta\n";
            //    } else {
            //        std::cerr << "DEBUG: pasta started (pid " << pasta_pid << ") attaching to " << container_pid << "\n";
            //    }
            //} else {
            //    std::cerr << "Skipping pasta launch — no valid container PID\n";
            //}

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
