#include "../include/tty_proxy_server.hpp"
#include "../include/network.hpp"
#include "../include/utils.hpp"

bool TTYProxyServer::m_running { false };
pid_t TTYProxyServer::m_server_pid { -1 };

// attach -> use quiver -a <container_id or container name>
// detach -> inside container ctrl-p then ctrl-q

int TTYProxyServer::start(const int master_fd, const pid_t monitor_pid, const std::string& sock_path) {
    if (!m_running) {
        return run(master_fd, monitor_pid, sock_path);
    }
    return 0;
}

int TTYProxyServer::run(const int master_fd, const pid_t monitor_pid, const std::string& sock_path) {
    Utils::ensure_dirs(sock_path);

    unlink(sock_path.c_str());

    int sfd{ socket(AF_UNIX, SOCK_STREAM, 0) };
    if (sfd == -1) Utils::handle_error("TTY Proxy Socket Creation failed");

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(sfd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(sfd);
        Utils::handle_error("TTY Proxy Socket binding failed");
    }

    chmod(sock_path.c_str(), S_IRUSR | S_IWUSR);

    if (listen(sfd, 1) == -1) {
        close(sfd);
        unlink(sock_path.c_str());
        Utils::handle_error("TTY Proxy listen failed");
    }

    m_running = true;

    while (true) {
        int status{};
        pid_t r{ waitpid(monitor_pid, &status, WNOHANG) };
        if (r == monitor_pid) {
            break;
        }

        pollfd pf{};
        pf.fd = sfd;
        pf.events = POLLIN;
        int rv{ poll(&pf, 1, 500) };

        if (rv < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (rv == 0) {
            continue;
        }

        int cfd{ accept(sfd, NULL, NULL) };
        if (cfd == -1) {
            if (errno == EINTR) continue;
            break;
        }

        bool client_connected { true };
        while (client_connected) {
            pid_t rr{ waitpid(monitor_pid, &status, WNOHANG) };
            if (rr == monitor_pid) {
                client_connected = false;
                m_running = false;
                break;
            }

            pollfd pf2[2]{};
            pf2[0].fd = master_fd;
            pf2[0].events = POLLIN;
            pf2[1].fd = cfd;
            pf2[1].events = POLLIN;

            int r2{ poll(pf2, 2, 500) };
            if (r2 < 0) {
                if (errno == EINTR) continue;
                client_connected = false;
                break;
            }
            if (r2 == 0) continue;

            if ((pf2[0].revents | pf2[1].revents) & (POLLERR | POLLHUP | POLLNVAL)) {
                if (pf2[0].revents & (POLLHUP | POLLERR)) {
                    m_running = false;
                }
                client_connected = false;
                break;
            }

            if (pf2[0].revents & POLLIN) {
                char buf[4096]{};
                ssize_t n{ read(master_fd, buf, sizeof(buf)) };
                if (n <= 0) {
                    m_running = false;
                    client_connected = false;
                } else if (write(cfd, buf, n) <= 0) {
                    client_connected = false;
                }
            }

            if (pf2[1].revents & POLLIN) {
                char buf[4096]{};
                ssize_t n{ read(cfd, buf, sizeof(buf)) };
                if (n <= 0) {
                    client_connected = false;
                } else if (write(master_fd, buf, n) <= 0) {
                    m_running = false;
                    client_connected = false;
                }
            }
        }
        close(cfd);

        if (!m_running) {
            break;
        }
    }
    return proxy_cleanup(sfd, master_fd, sock_path);
}

int TTYProxyServer::proxy_cleanup(const int sfd, const int master_fd, const std::string& sock_path) {
    if (sfd >= 0) close(sfd);
    if (master_fd >= 0) close(master_fd);
    m_running = false;
    kill(Network::get_net_pid(),SIGTERM);
    return 0;
}
int TTYProxyServer::reattach_to_socket(const std::string& sock_path) {
    int sfd{ socket(AF_UNIX, SOCK_STREAM, 0) };
    if (sfd == -1) Utils::handle_error("Attach Socket failed");

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.data(), sizeof(addr.sun_path) - 1);

    if (connect(sfd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        close(sfd);
        Utils::handle_error("Attach Socket Connection failed");
    }

    termios oldt{};
    if (tcgetattr(STDIN_FILENO, &oldt) == -1) {
        close(sfd);
        Utils::handle_error("Attach tcgetattr failed");
    }

    termios newt { oldt };
    cfmakeraw(&newt);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    bool saw_ctrlp { false };
    bool should_run { true };
    bool user_detached { false };

    while (should_run) {
        pollfd fds[2]{};
        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = sfd;
        fds[1].events = POLLIN;

        int rc = poll(fds, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if ((fds[0].revents | fds[1].revents) & (POLLERR | POLLHUP | POLLNVAL)) {
            break;
        }

        if (fds[0].revents & POLLIN) {
            char buf[4096];
            ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) {
                should_run = false;
                break;
            }

            for (ssize_t i = 0; i < n; ++i) {
                unsigned char c = static_cast<unsigned char>(buf[i]);

                if (saw_ctrlp) {
                    if (c == 0x11) {
                        should_run = false;
                        user_detached = true;
                        break;
                    }
                    unsigned char seq[2] { 0x10, c };
                    if (write(sfd, seq, 2) <= 0) {
                        should_run = false;
                        break;
                    }
                    saw_ctrlp = false;
                } else if (c == 0x10) {
                    saw_ctrlp = true;
                } else {
                    if (write(sfd, &c, 1) <= 0) {
                        should_run = false;
                        break;
                    }
                }
            }
        }

        if (!should_run) break;

        if (fds[1].revents & POLLIN) {
            char buf[4096];
            ssize_t n { read(sfd, buf, sizeof(buf)) };
            if (n <= 0) {
                should_run = false;
                break;
            }
            if (write(STDOUT_FILENO, buf, n) <= 0) {
                should_run = false;
                break;
            }
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    close(sfd);

    if (user_detached) {
        fprintf(stderr, "\r\n[detached from container]\r\n");
    }
    else {
        fprintf(stderr, "\r\n[session terminated by server]\r\n");
    }
    return 0;
}

TTYProxyServer::~TTYProxyServer() {}
