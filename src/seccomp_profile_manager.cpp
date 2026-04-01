#include "seccomp_profile_manager.hpp"
#include <cstdint>
#include <seccomp.h>
#include <stdexcept>
#include <format>

SeccompProfileManager::SeccompProfileManager(uint32_t default_action) {
        m_ctx = seccomp_init(default_action);
        if (m_ctx == nullptr) {
                throw std::runtime_error("Seccomp Profile Manager Error: Failed to initialize filter context.");
        }
}

auto SeccompProfileManager::add_rule(uint32_t action, const std::string& syscall_name) -> void {
        int syscall_num{seccomp_syscall_resolve_name(syscall_name.c_str())};
        if (syscall_num == __NR_SCMP_ERROR) {
                throw std::runtime_error(std::format("Seccomp Profile Manager Error: Unknown syscall '{}'", syscall_name));
        }

        if (seccomp_rule_add(m_ctx, action, syscall_num, 0) < 0) {
                throw std::runtime_error(std::format("Seccomp Profile Manager Error: Failed to add rule for '{}'", syscall_name));
        }
}

auto SeccompProfileManager::apply() -> void {
        if (seccomp_load(m_ctx) < 0) {
                throw std::runtime_error("Seccomp Profile Manager Error: Failed to load profile into kernel.");
        }
}

SeccompProfileManager::~SeccompProfileManager() {
        if (m_ctx != nullptr) {
                seccomp_release(m_ctx);
        }
}
