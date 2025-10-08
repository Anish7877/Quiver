#pragma once

#include <vector>
#include <iostream>
#include <sched.h>
#include <string>
#include <cstring>
#include <fstream>
#include <cassert>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string>
#include <sys/types.h>
#include <pty.h>
#include <utmp.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <pwd.h>

struct ContainerArgs {
    std::string hostname{};
    std::string rootfs_path{};
    std::string program_path{};
    int slave_fd{};
    std::string filesystem_dir{};
};

// class Process {
//     public:
//         Process() = default;
//         ~Process() {};
//         int start(const std::string& new_hostname,const std::vector<std::string>& volumes,const std::string& container_name,const std::string& path);
//         int run_container(ContainerArgs* arg);
//         pid_t pid() const { return m_child_pid; }
//     private:
//         void setup_user_namespace();
//         int run(const std::string& path);
//         pid_t m_child_pid{};
//         
//         std::string m_new_fs{};
//         std::vector<std::string> m_volumes{};
// };

class Process {
public:
    Process() = default;
    ~Process() {};
    int start(const std::string& new_hostname, const std::vector<std::string>& volumes, const std::string& filesystem_path, const std::string& path);
    int run_container(ContainerArgs* arg);
    pid_t pid() const { return m_child_pid; }

private:
    void setup_user_namespace();
    int run(const std::string& path, std::string& container_id);
    pid_t m_child_pid{};
    std::string m_new_fs{};
    std::string m_new_hostname{};
    std::vector<std::string> m_volumes{};
    std::string m_container_id{}; // <-- ADD THIS LINE
};