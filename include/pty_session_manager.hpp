#pragma once
#include <termios.h>
#include <string>

class PtySessionManager {
        public:
                PtySessionManager() = default;
                ~PtySessionManager();
                PtySessionManager(const PtySessionManager&) = delete;
                PtySessionManager(PtySessionManager&&) = delete;
                auto operator=(const PtySessionManager&) -> PtySessionManager& = delete;
                auto operator=(PtySessionManager&&) -> PtySessionManager& = delete;

                auto setup_pty() -> void;
                auto attach_to_container_slave() -> void;
                auto get_master_fd() -> int { return m_master_fd; }
                auto ok() -> bool { return m_ok; }
                auto get_error() -> std::string { return m_error; }
        private:
                auto enable_raw_mode() -> bool;
                auto disable_raw_mode() -> bool;
                termios m_orig_term{};
                std::string m_slave_name{};
                std::string m_error{};
                int m_slave_fd{-1};
                int m_master_fd{-1};
                bool m_ok;
};
