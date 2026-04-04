#include "seccomp_profile_manager.hpp"
#include <stdexcept>
#include <format>
#include <iostream>

SeccompProfileManager::SeccompProfileManager(const OCIRuntime::Seccomp& config) {

        uint32_t def_action{resolve_action(config.default_action, config.default_errno)};
        m_ctx = seccomp_init(def_action);
        if (m_ctx == nullptr) [[unlikely]] {
                throw std::runtime_error("Seccomp Profile Manager Error: Failed to initialize filter context.");
        }

        for (const auto& arch_str : config.archs) {
                auto it{OCIRuntime::SCMP_ARCH_STR_MAP.find(arch_str)};
                if (it != OCIRuntime::SCMP_ARCH_STR_MAP.end()) {
                        seccomp_arch_add(m_ctx, it->second);
                }
        }

        for (const auto& rule : config.syscalls) {
                uint32_t action{resolve_action(rule.action, rule.errno_ret)};
                std::vector<scmp_arg_cmp> cmp_args;
                cmp_args.reserve(rule.args.size());

                for (const auto& arg : rule.args) {
                        auto op_it{OCIRuntime::SCMP_SYSCALL_OP_STR_MAP.find(arg.op)};
                        if (op_it == OCIRuntime::SCMP_SYSCALL_OP_STR_MAP.end()) [[unlikely]] {
                                throw std::runtime_error(std::format("Unknown seccomp operator: {}", arg.op));
                        }

                        cmp_args.push_back(SCMP_CMP(
                                arg.index,
                                static_cast<scmp_compare>(op_it->second),
                                arg.value,
                                arg.value_two
                        ));
                }
                for (const auto& name : rule.names) {
                        int syscall_num{seccomp_syscall_resolve_name(name.c_str())};
                        if (syscall_num == __NR_SCMP_ERROR) [[unlikely]] {
                                std::cerr << std::format("WARN: Seccomp ignoring unknown syscall: {}\n", name);
                                continue;
                        }
                        if (seccomp_rule_add_array(m_ctx, action, syscall_num, cmp_args.size(), cmp_args.data()) < 0) [[unlikely]] {
                                throw std::runtime_error(std::format("Seccomp Error: Failed to add rule for '{}'", name));
                        }
                }
        }
}

auto SeccompProfileManager::apply() -> void {
        if (seccomp_load(m_ctx) < 0) [[unlikely]] {
                throw std::runtime_error("Seccomp Profile Manager Error: Failed to load profile into kernel.");
        }
}

auto SeccompProfileManager::resolve_action(const std::string& action_str, uint32_t errno_ret) const -> uint32_t {
        if (action_str == "SCMP_ACT_KILL" || action_str == "SCMP_ACT_KILL_PROCESS") return SCMP_ACT_KILL_PROCESS;
        if (action_str == "SCMP_ACT_KILL_THREAD") return SCMP_ACT_KILL_THREAD;
        if (action_str == "SCMP_ACT_TRAP") return SCMP_ACT_TRAP;
        if (action_str == "SCMP_ACT_ERRNO") return SCMP_ACT_ERRNO(errno_ret);
        if (action_str == "SCMP_ACT_TRACE") return SCMP_ACT_TRACE(errno_ret);
        if (action_str == "SCMP_ACT_ALLOW") return SCMP_ACT_ALLOW;
        if (action_str == "SCMP_ACT_LOG") return SCMP_ACT_LOG;

        throw std::runtime_error(std::format("Unknown seccomp action: {}", action_str));
}

SeccompProfileManager::~SeccompProfileManager() {
        if (m_ctx != nullptr) [[unlikely]] {
                seccomp_release(m_ctx);
        }
}
