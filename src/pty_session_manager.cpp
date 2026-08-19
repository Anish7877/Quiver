#include "pty_session_manager.hpp"
#include <cstdlib>
#include <format>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>

auto PtySessionManager::setup_pty() -> void {
        m_master_fd = open("/dev/ptmx", O_NOCTTY | O_RDWR);
        if (m_master_fd == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: cannot open /dev/ptmx.\n";
                return;
        }

        if (grantpt(m_master_fd) == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: grantpt failed.\n";
                close(m_master_fd);
                return;
        }
        if (unlockpt(m_master_fd) == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: unlockpt failed.\n";
                close(m_master_fd);
                return;
        }

        char* slave_ptr{ptsname(m_master_fd)};
        if (slave_ptr == nullptr) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: cannot get slave name.\n";
                close(m_master_fd);
                return;
        }

        m_slave_name = slave_ptr;
        m_slave_fd = open(m_slave_name.c_str(), O_RDWR);
        if (m_slave_fd == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: cannot open slave pts.\n";
                close(m_master_fd);
                return;
        }

        m_ok = true;
}

auto PtySessionManager::send_master_fd(int control_sock, int fd) -> void {
        msghdr msg{0};

        char dummy_data[1]{'F'};
        iovec iov[1];
        iov[0].iov_base = dummy_data;
        iov[0].iov_len = 1;

        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        union {
                char buffer[CMSG_SPACE(sizeof(int))];
                struct cmsghdr align;
        } control_msg{};

        msg.msg_control = control_msg.buffer;
        msg.msg_controllen = sizeof(control_msg.buffer);

        cmsghdr* cmsg{CMSG_FIRSTHDR(&msg)};
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));

        memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

        if (sendmsg(control_sock, &msg, 0) == -1) {
                m_ok = false;
                m_error = std::format("Pty Session Manager: sendmsg failed -> '{}'.\n", std::strerror(errno));
                return;
        }
        m_ok = true;
        m_error = "";
}

auto PtySessionManager::recv_master_fd(int control_sock) -> int {
        msghdr msg{0};

        char dummy_data[1]{'F'};
        iovec iov[1];
        iov[0].iov_base = dummy_data;
        iov[0].iov_len = 1;

        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        union {
                char buffer[CMSG_SPACE(sizeof(int))];
                struct cmsghdr align;
        } control_msg{};

        msg.msg_control = control_msg.buffer;
        msg.msg_controllen = sizeof(control_msg.buffer);

        if (recvmsg(control_sock, &msg, 0) <= 0) {
                m_ok = false;
                m_error = std::format("Pty Session Manager: recvmsg failed -> '{}'.\n", std::strerror(errno));
                return -1;
        }

        cmsghdr* cmsg{CMSG_FIRSTHDR(&msg)};

        if(cmsg != nullptr && cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                int received_fd{-1};
                memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
                m_ok = true;
                m_error = "";
                return received_fd;
        }
        m_ok = false;
        m_error = "Pty Session Manager: cmsg header properties are not identical.\n";
        return -1;
}

auto PtySessionManager::enable_raw_mode() -> void {
        if (tcgetattr(STDIN_FILENO, &m_orig_term) == -1) {
                m_ok = false;
                m_error = "Pty Session Manager: tcgetattr failed.\n";
                return;
        }
        termios new_term{m_orig_term};
        cfmakeraw(&new_term);
        atexit([]() {
                        auto& pty{PtySessionManager::get_instance()};
                        pty.disable_raw_mode();
                });
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term) == -1) {
                m_ok = false;
                m_error = "Pty Session Manager: tcsetattr failed for enable.\n";
                return;
        }
        m_ok = true;
        m_error = "";
}

auto PtySessionManager::disable_raw_mode() -> void {
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig_term) == -1) {
                m_ok = false;
                m_error = "Pty Session Manager: tcsetattr failed for disable.\n";
                return;
        }
        m_ok = true;
        m_error = "";
}

PtySessionManager::~PtySessionManager() {
        if (m_slave_fd != -1) {
                close(m_slave_fd);
        }
        if (m_master_fd != -1) {
                close(m_master_fd);
        }
}
