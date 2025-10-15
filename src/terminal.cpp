#include "../include/terminal.hpp"
#include "../include/utils.hpp"
#include <cstdlib>
#include <pty.h>
#include <termios.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <cstring>
#include <csignal>
#include <fcntl.h>

termios Terminal::m_orig_term{};
pid_t Terminal::m_container_pid{-1};
volatile bool Terminal::m_running{ false };

void Terminal::start_pty_session(PtyArgs& args){
    if(openpty(&args.master_fd, &args.slave_fd, args.slave_name, nullptr, &args.window_size) == ERR)
        Utils::handle_error("Unable to open a new pty session");
}

void Terminal::redirect_io(const int &slave_fd){
    if(ioctl(slave_fd, TIOCSCTTY, 0) == ERR)
        Utils::handle_error("ioctl TCIOCSCTTY");
    if(dup2(slave_fd, STDIN_FILENO) == ERR)
        Utils::handle_error("Redirect stdin to slave file descriptor");
    if(dup2(slave_fd, STDOUT_FILENO) == ERR)
        Utils::handle_error("Redirect stdout to slave file descriptor");
    if(dup2(slave_fd, STDERR_FILENO) == ERR)
        Utils::handle_error("Redirect stderr to slave file descriptor");
}

void Terminal::start_server(const PtyArgs& args, const pid_t& container_pid, const pid_t& manager_pid){
    //shim();
    m_container_pid = container_pid;
    m_running = true;
    close(args.slave_fd);

    signal(SIGCHLD, sigchld_handler);
    int sock_fd{ socket(AF_UNIX, SOCK_STREAM, 0) };
    if (sock_fd == -1) Utils::handle_error("Unable to create a socket from server side");

    const std::string sock_path{ Utils::get_sock_path(manager_pid) };

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    unlink(sock_path.c_str());
    if (bind(sock_fd, (sockaddr*)&addr, sizeof(addr)) == ERR) Utils::handle_error("Unable to bind to a socket");
    if (listen(sock_fd, 5) == ERR) Utils::handle_error("Unable to listen to a socket ");

    while (m_running) {
        fd_set read_fds{};
        FD_ZERO(&read_fds);
        FD_SET(sock_fd, &read_fds);
        timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret{ select(sock_fd + 1, &read_fds, NULL, NULL, &tv) };
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;

        int client_sock{ accept(sock_fd, NULL, NULL) };
        if (client_sock == -1) continue;

        pid_t handler_pid{ fork() };
        if (handler_pid == 0) {
            close(sock_fd);
            send_fd(client_sock, args.master_fd);
            close(client_sock);
            exit(0);
        } else {
            close(client_sock);
            waitpid(handler_pid, NULL, 0);
        }
    }
    close(args.master_fd);
    close(sock_fd);
    unlink(sock_path.c_str());

}

void Terminal::connect_to_server(const pid_t& manager_pid){
    std::string sock_path{ Utils::get_sock_path(manager_pid) };
    int sock_fd { socket(AF_UNIX, SOCK_STREAM, 0) };
    if (sock_fd == ERR) Utils::handle_error("Unable to create socket from client side");

    sockaddr_un addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        Utils::handle_error("Connection failed to " + sock_path);
    }

    int master_fd{ receive_fd(sock_fd) };
    close(sock_fd);

    if(master_fd == ERR) Utils::handle_error("Failed to receive master fd");

    enable_raw_mode();
    atexit(restore_state);

    enum { NORMAL, ESCAPE } state{ NORMAL };

    while (true) {
        fd_set read_fds{};
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(master_fd, &read_fds);

        int max_fd { std::max(STDIN_FILENO, master_fd) };
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) Utils::handle_error("Select error from client side");

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char c{};
            ssize_t n { read(STDIN_FILENO, &c, 1) };
            if (n <= 0) break;

            if (state == NORMAL) {
                if (c == '\x10') {
                    state = ESCAPE;
                } else {
                    if (write(master_fd, &c, 1) != 1) break;
                }
            } else if (state == ESCAPE) {
                if (c == '\x04'){
                    break;
                } else {
                    char sequence[2]{'\x10', c};
                    if (write(master_fd, sequence, 2) != 2) break;
                    state = NORMAL;
                }
            }
        }

        if (FD_ISSET(master_fd, &read_fds)) {
            char buf[256]{};
            ssize_t n{ read(master_fd, buf, 256) };
            if (n <= 0) break;
            if (write(STDOUT_FILENO, buf, n) != n) break;
        }
    }
    close(master_fd);
}

void Terminal::sigchld_handler(int signum){
    int status{};
    pid_t reaped_pid { waitpid(m_container_pid, &status, WNOHANG) };
    if (reaped_pid == m_container_pid && (WIFEXITED(status) || WIFSIGNALED(status))) {
        m_running = false;
    }
    restore_state();
}

void Terminal::enable_raw_mode(){
    if(tcgetattr(STDIN_FILENO, &m_orig_term) == ERR)
        Utils::handle_error("tcgetattr original terminal");
    termios raw{};
    cfmakeraw(&raw);
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == ERR)
        Utils::handle_error("tcsetattr raw terminal");
}

void Terminal::send_fd(const int& socket, const int& fd_to_send){
    msghdr msg{};
    char buf[1]{ 0 };
    iovec iov[1]{};
    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int *)CMSG_DATA(cmsg) = fd_to_send;
    if (sendmsg(socket, &msg, 0) == ERR){
        Utils::handle_error("Could not send message to slave file descriptor");
    }
}

int Terminal::receive_fd(const int& socket){
    msghdr msg{};
    char buf[1]{};
    iovec iov[1];
    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    char cmsg_buf[CMSG_SPACE(sizeof(int))]{};
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);
    if (recvmsg(socket, &msg, 0) == ERR) {
        Utils::handle_error("Unable to message from socket");
    }
    cmsghdr *cmsg{ CMSG_FIRSTHDR(&msg) };
    return *(int *)CMSG_DATA(cmsg);
}

void Terminal::shim(){
    pid_t pid { fork() };
    if (pid == ERR) Utils::handle_error("Cannot create container shim");
    if (pid > 0) exit(EXIT_SUCCESS); // parent exits

    if (setsid() == ERR) Utils::handle_error("Shim setsid");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

void Terminal::restore_state(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig_term) == ERR)
        Utils::handle_error("Cannot restore terminal original state");
}

Terminal::~Terminal(){
}
