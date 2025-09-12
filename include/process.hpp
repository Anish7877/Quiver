#pragma once

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
#include <string_view>
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

class Process {
    public:
        Process() = default;
        ~Process();
        int start(const std::string& new_hostname,const std::string& container_name,std::string_view path);
        int attach();
        int detach();
        int reattach();
        int stop();
        pid_t pid() const { return m_child_pid; }
    private:
        static int handle_error(std::string_view err);
        static int run(std::string_view path);
        static int write_file(const std::string& path,const std::string& str);
        static int pivot_root();
        static std::string get_filesystem_dir(pid_t pid);
        static void ensure_dirs(const std::string& dir);
        static pid_t m_child_pid;
        static uid_t m_uid;
        static std::string m_new_hostname;
        static std::string m_new_fs;
};
