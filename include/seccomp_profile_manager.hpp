#pragma once
#include <cstdint>
#include <string>
#include <seccomp.h>

class SeccompProfileManager {
        public:
                explicit SeccompProfileManager(uint32_t);
                ~SeccompProfileManager();
                SeccompProfileManager(const SeccompProfileManager&) = delete;
                SeccompProfileManager(SeccompProfileManager&&) = delete;
                auto operator=(const SeccompProfileManager&) -> SeccompProfileManager& = delete;
                auto operator=(SeccompProfileManager&&) -> SeccompProfileManager& = delete;

                auto add_rule(uint32_t, const std::string&) -> void;
                auto apply() -> void;
        private:
                scmp_filter_ctx m_ctx{};
};
