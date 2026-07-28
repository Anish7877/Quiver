#include "signal_handler.hpp"
#include <signal.h>

pid_t SignalHandler::m_target_pid{-1};
auto SignalHandler::set_target_pid(pid_t pid) -> void {
        m_target_pid = pid;
}

auto SignalHandler::handle_signals() -> void {
        struct sigaction sa{};
        sa.sa_handler = &SignalHandler::signal_callback;
        sa.sa_flags = SA_RESTART;
        sigemptyset(&sa.sa_mask);

        constexpr int proxy_signals[]{
                SIGINT,
                SIGPWR,
                SIGTERM,
                SIGHUP,
                SIGQUIT,
                SIGUSR1,
                SIGUSR2
        };
        for (int sig : proxy_signals) {
                sigaction(sig, &sa, nullptr);
        }
}

auto SignalHandler::signal_callback(int signum) -> void {
        if (m_target_pid > 0) {
                kill(m_target_pid, signum);
        }
}
