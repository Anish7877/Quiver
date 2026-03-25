#include "pty_session_manager.hpp"
#include <fcntl.h>
#include <unistd.h>

auto PtySessionManager::setup_pty() -> void {
        m_master_fd = open("/dev/ptmx", O_NOCTTY | O_RDWR);
        if (m_master_fd == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: cannot open /dev/ptmx.";
                return;
        }

        if (grantpt(m_master_fd) == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: grantpt failed.";
                close(m_master_fd);
                return;
        }
        if (unlockpt(m_master_fd) == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: unlockpt failed.";
                close(m_master_fd);
                return;
        }

        char* slave_ptr{ptsname(m_master_fd)};
        if (slave_ptr == nullptr) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: cannot get slave name.";
                close(m_master_fd);
                return;
        }
        m_slave_name = slave_ptr;
        m_slave_fd = open(m_slave_name.c_str(), O_NOCTTY | O_RDWR);
        if (m_slave_fd == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Pty Session Manager Error: cannot open slave pts.";
                close(m_master_fd);
                return;
        }

        dup2(m_slave_fd, STDIN_FILENO);
        dup2(m_slave_fd, STDOUT_FILENO);
        dup2(m_slave_fd, STDERR_FILENO);

        close(m_slave_fd);
        m_slave_fd = -1;
        m_ok = true;
}
