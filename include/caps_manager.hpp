#pragma once
#include <sys/capability.h>
#include <string>
#include <vector>

class CapsManager {
        public:
                explicit CapsManager();
                ~CapsManager();
                CapsManager(const CapsManager&) = delete;
                CapsManager(CapsManager&&) = delete;
                auto operator=(const CapsManager&) -> CapsManager& = delete;
                auto operator=(CapsManager&&) -> CapsManager& = delete;

                auto add_capability(const std::string&) -> void;
                auto apply() -> void;
        private:
                cap_t m_cap{};
                std::vector<cap_value_t> m_caps_list{};
};
