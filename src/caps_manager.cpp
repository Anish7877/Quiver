#include "caps_manager.hpp"
#include <format>
#include <stdexcept>
#include <sys/prctl.h>
#include <iostream>

CapsManager::CapsManager(const OCIRuntime::Capabilities& capabilities) {
        m_cap = cap_init();
        if (m_cap == nullptr) [[unlikely]] {
                throw std::runtime_error("Capabilities Manager Error: Failed to initialize cap state.\n");
        }
        m_permitted.reserve(capabilities.permitted.size());
        m_effective.reserve(capabilities.effective.size());
        m_inheritable.reserve(capabilities.inheritable.size());
        m_ambient.reserve(capabilities.ambient.size());
        m_bounding.reserve(capabilities.bounding.size());

        resolve_caps(capabilities.permitted, m_permitted);
        resolve_caps(capabilities.effective, m_effective);
        resolve_caps(capabilities.inheritable, m_inheritable);
        resolve_caps(capabilities.ambient, m_ambient);
        resolve_caps(capabilities.bounding, m_bounding);
}


auto CapsManager::apply() -> void {
        if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) == -1) {
                throw std::runtime_error("Capabilities Manager Error: Failed to set KEEPCAPS");
        }

        for (std::size_t i{0}; i <= CAP_LAST_CAP; ++i) {
                cap_value_t cap_value{static_cast<cap_value_t>(i)};
                if (std::find(m_bounding.begin(), m_bounding.end(), cap_value) == m_bounding.end()) {
                        if (prctl(PR_CAPBSET_DROP, cap_value, 0, 0, 0) == -1) {
                                if (errno != EINVAL && errno != EPERM) { // EPERM means it's already gone
                                        throw std::runtime_error(std::format("Capabilities Manager Error: Failed to drop cap {} from bounding set.\n", i));
                                }
                        }
                }
        }

        cap_clear(m_cap);

        auto cap_set{[&](cap_flag_t flag, const std::vector<cap_value_t>& caps) {
                if (caps.empty()) return;
                if (cap_set_flag(m_cap, flag, static_cast<int>(caps.size()), caps.data(), CAP_SET) == -1) {
                        throw std::runtime_error("Capabilities Manager Error: Failed to set caps flag.\n");
                }
        }};

        cap_set(CAP_EFFECTIVE, m_effective);
        cap_set(CAP_PERMITTED, m_permitted);
        cap_set(CAP_INHERITABLE, m_inheritable);

        if (cap_set_proc(m_cap) == -1) {
                if (errno == EPERM) {
                        std::cerr << "WARN: Some requested capabilities were rejected by the host kernel.\n";
                }
                else {
                        throw std::runtime_error("Capabilities Manager Error: Failed to apply capabilities.\n");
                }
        }

        for (const auto& cap : m_ambient) {
                if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) == -1) {
                        if (errno == EPERM) {
                                std::cerr << std::format("WARN: System denied Ambient capability {}.\n", cap);
                                continue;
                        }
                        throw std::runtime_error(std::format("Capabilities Manager Error: Failed to raise ambient cap {}.\n", cap));
                }
        }
}

auto CapsManager::resolve_caps(const std::vector<std::string>& caps, std::vector<cap_value_t>& target) -> void {
        for (const auto& cap_str : caps) {
                cap_value_t cap_val{};

                if (cap_from_name(cap_str.c_str(), &cap_val) == 0) {
                        target.emplace_back(cap_val);
                } else [[unlikely]] {
                        std::cerr << std::format("WARN: Ignored unknown or unsupported capability: {}\n", cap_str);
                }
        }
}

CapsManager::~CapsManager() {
        if (m_cap != nullptr) [[likely]] {
                cap_free(m_cap);
        }
}
