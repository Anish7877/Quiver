#pragma once

#include <libgen.h>
#include <vector>
#include <sched.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <termios.h>
#include <cerrno>


class TTYProxyServer {
    public:
        TTYProxyServer() = default;
        int start(const int master_fd,const pid_t monitor_pid,const std::string& sock_path);
        bool is_running() const { return m_running; };
        int reattach_to_socket(const std::string& socket_path);
        ~TTYProxyServer();
    private:
        static int run(const int master_fd,const pid_t monitor_pid,const std::string& sock_path);
        static int proxy_cleanup(const int sfd,const int master_fd,const std::string& sock_path);
        static bool m_running;
        static pid_t m_server_pid;
};
