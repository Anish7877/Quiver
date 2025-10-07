#pragma once
#define SLAVE_LENGTH 128
#include <termios.h>
#include <sys/ioctl.h>

class Terminal{
    public:
        struct PtyArgs{
            int master_fd{};
            int slave_fd{};
            char slave_name[SLAVE_LENGTH]{};
            winsize window_size{};
        };
        explicit Terminal() = default;
        ~Terminal();
        void start_pty_session(PtyArgs& args);
        void redirect_io(const int& slave_fd);
        void start_server(const PtyArgs& args, const pid_t& container_pid);
        void connect_to_server(const pid_t& container_pid);
    private:
        static void sigchld_handler(int signum);
        void enable_raw_mode();
        void send_fd(const int& socket, const int& fd_to_send);
        int receive_fd(const int& socket);
        void shim();
        void restore_state();
        static termios m_orig_term;
        static pid_t m_container_pid;
        static bool m_running;
};
