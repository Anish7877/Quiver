#pragma once
#define SLAVE_LENGTH 128
#include <sys/ioctl.h>

struct PtyArgs{
    int master_fd{};
    int slave_fd{};
    char slave_name[SLAVE_LENGTH]{};
    winsize window_size{};
};
namespace Terminal{
    void start_pty_session(PtyArgs& args);
    void link_pty_proxy();
    void launch_terminal();
    void terminal_cleanup();
}
