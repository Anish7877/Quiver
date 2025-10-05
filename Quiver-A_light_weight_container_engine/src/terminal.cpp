#include "../include/terminal.hpp"
#include "../include/utils.hpp"
#include <cstddef>
#include <pty.h>


void Terminal::start_pty_session(PtyArgs& args){
    if(openpty(&args.master_fd, &args.slave_fd, args.slave_name, nullptr, &args.window_size) == ERR)
        Utils::handle_error("Unable to open a new pty session");
}
void Terminal::link_pty_proxy(); // research
void Terminal::launch_terminal();
void Terminal::terminal_cleanup();// after terminal done and container process ended
