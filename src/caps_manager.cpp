#include "caps_manager.hpp"
#include <format>
#include <stdexcept>
#include <sys/prctl.h>
#include <iostream>

CapsManager::CapsManager(const OCIRuntime::Capabilities& capabilities) {
        m_cap = cap_init();
        if (m_cap == nullptr) [[unlikely]] {
                throw std::runtime_error("Capabilities Manager Error: Failed to initialize cap state.");
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
        for (std::size_t i{0}; i <= CAP_LAST_CAP; ++i) {
                cap_value_t cap_value{static_cast<cap_value_t>(i)};
                if (std::find(m_bounding.begin(), m_bounding.end(), cap_value) == m_bounding.end()) {
                        if (prctl(PR_CAPBSET_DROP, cap_value, 0, 0, 0) == -1) [[unlikely]] {
                                if (errno != EINVAL) [[unlikely]] {
                                        throw std::runtime_error(std::format("Capabilities Manager Error: Failed to drop cap {} from bounding set. Errno: {}", i, errno));
                                }
                        }
                }
        }

        cap_clear(m_cap);
        if (cap_set_flag(m_cap, CAP_EFFECTIVE, static_cast<int>(m_effective.size()), m_effective.data(), CAP_SET) == -1 ||
            cap_set_flag(m_cap, CAP_PERMITTED, static_cast<int>(m_permitted.size()), m_permitted.data(), CAP_SET) == -1 ||
            cap_set_flag(m_cap, CAP_INHERITABLE, static_cast<int>(m_inheritable.size()), m_inheritable.data(), CAP_SET) == -1) [[unlikely]] {
                throw std::runtime_error("Capabilities Manager Error: Failed to set caps flag.");
        }

        if (cap_set_proc(m_cap) == -1) [[unlikely]] {
                throw std::runtime_error("Capabilities Manager Error: Failed to apply capabilities to process.");
        }

        for (const auto& cap : m_ambient) {
                if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) == -1) [[unlikely]] {
                        throw std::runtime_error(std::format("Capabilities Manager Error: Failed to raise ambient cap {}. Errno: {}", cap, errno));
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
        if (m_cap != nullptr) [[unlikely]] {
                cap_free(m_cap);
        }
}
