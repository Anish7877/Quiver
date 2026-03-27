#pragma once
#include "singleton.hpp"
#include <termios.h>
#include <string>

class PtySessionManager : public Singleton<PtySessionManager>{
        friend class Singleton<PtySessionManager>;
        private:
                PtySessionManager() = default;
                ~PtySessionManager();
        public:
                PtySessionManager(const PtySessionManager&) = delete;
                PtySessionManager(PtySessionManager&&) = delete;
                auto operator=(const PtySessionManager&) -> PtySessionManager& = delete;
                auto operator=(PtySessionManager&&) -> PtySessionManager& = delete;

                auto setup_pty() -> void;
                auto send_master_fd(int, int) -> void;
                auto recv_master_fd(int) -> int;
                auto ok() -> bool { return m_ok; }
                auto get_master_fd() const -> int { return m_master_fd; }
                auto get_error() -> std::string { return m_error; }
                auto enable_raw_mode() -> void;
                auto disable_raw_mode() -> void;
        private:
                termios m_orig_term{};
                std::string m_slave_name{};
                std::string m_error{};
                int m_slave_fd{-1};
                int m_master_fd{-1};
                bool m_ok{true};
};
