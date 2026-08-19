#pragma once
#include "oci_runtime.hpp"
#include <seccomp.h>
#include <string>

class SeccompProfileManager {
        public:
                explicit SeccompProfileManager(const OCIRuntime::Seccomp&);
                ~SeccompProfileManager();
                SeccompProfileManager(const SeccompProfileManager&) = delete;
                auto operator=(const SeccompProfileManager&) -> SeccompProfileManager& = delete;

                auto apply() -> void;
        private:
                scmp_filter_ctx m_ctx{nullptr};
                auto resolve_action(const std::string&, uint32_t) const -> uint32_t;
};
