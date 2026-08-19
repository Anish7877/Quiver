#pragma once
#include <unistd.h>
#include "singleton.hpp"

class SignalHandler : public Singleton<SignalHandler> {
        friend class Singleton<SignalHandler>;
        private:
                SignalHandler() = default;
                ~SignalHandler() = default;
        public:
                SignalHandler(const SignalHandler&) = delete;
                SignalHandler(const SignalHandler&&) = delete;
                auto operator=(const SignalHandler&) -> void = delete;
                auto operator=(const SignalHandler&&) -> void = delete;

                auto set_target_pid(pid_t) -> void;
                auto handle_signals() -> void;
        private:
                static auto signal_callback(int) -> void;
                static pid_t m_target_pid;
};
