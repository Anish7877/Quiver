#pragma once
#define SLAVE_LENGTH 128
#include <string>
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
        void start_server(const PtyArgs& args, const std::string& container_id, const pid_t& container_pid);
        void connect_to_server(const pid_t& container_pid);
    private:
        void cleanup(int sfd, int master_fd);
        static termios m_orig_term;
        static pid_t m_container_pid;
        static volatile bool m_running;
        static bool m_raw_mode_enabled;
};
