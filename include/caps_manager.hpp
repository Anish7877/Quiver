#pragma once
#include "oci_runtime.hpp"
#include <sys/capability.h>

class CapsManager {
        public:
                explicit CapsManager(const OCIRuntime::Capabilities&);
                ~CapsManager();
                CapsManager(const CapsManager&) = delete;
                CapsManager(CapsManager&&) = delete;
                auto operator=(const CapsManager&) -> CapsManager& = delete;
                auto operator=(CapsManager&&) -> CapsManager& = delete;

                auto apply() -> void;
        private:
                auto resolve_caps(const std::vector<std::string>&, std::vector<cap_value_t>&) -> void;
                std::vector<cap_value_t> m_permitted{};
                std::vector<cap_value_t> m_effective{};
                std::vector<cap_value_t> m_inheritable{};
                std::vector<cap_value_t> m_ambient{};
                std::vector<cap_value_t> m_bounding{};
                cap_t m_cap{};
};
