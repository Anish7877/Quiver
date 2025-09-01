#pragma once

#include <sched.h>
#include <string>
#include <string_view>
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
        int start(int master_fd,pid_t monitor_pid,const std::string& sock_path);
        bool is_running() const { return m_running; };
        int reattach_to_socket(std::string_view socket_path);
        std::string get_sock_path(pid_t pid);
        static int detach_from_socket(int cfd);
        ~TTYProxyServer();
    private:
        static int handle_error(std::string_view err);
        static void ensure_dirs(const std::string& dir);
        std::string get_sock_dir(pid_t pid);
        static int run(int master_fd,pid_t monitor_pid,const std::string& sock_path);
        static int proxy_cleanup(int sfd,int master_fd,const std::string& sock_path);
        int client_cleanup(const termios& oldt,int sfd);
        static bool m_running;
        static pid_t m_server_pid;
};
