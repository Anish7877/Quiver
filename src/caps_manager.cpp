#include "caps_manager.hpp"
#include <algorithm>
#include <format>
#include <linux/prctl.h>
#include <stdexcept>
#include <sys/prctl.h>

CapsManager::CapsManager() {
        m_cap = cap_init();
        if (m_cap == nullptr) {
                throw std::runtime_error("Capabilities Manager Error: Failed to initialize cap state.");
        }
}

auto CapsManager::add_capability(const std::string& cap) -> void {
        cap_value_t cap_value{};

        if (cap_from_name(cap.c_str(), &cap_value) == -1) {
                throw std::runtime_error(std::format("Capabilities Manager Error: Unknown capability '{}'.", cap));
        }
        m_caps_list.emplace_back(cap_value);
}

auto CapsManager::apply() -> void {
        if (m_caps_list.empty()) {
                cap_clear(m_cap);
        }
        else {
                if (cap_set_flag(m_cap, CAP_EFFECTIVE, m_caps_list.size(), m_caps_list.data(), CAP_SET) == -1 ||
                    cap_set_flag(m_cap, CAP_PERMITTED, m_caps_list.size(), m_caps_list.data(), CAP_SET) == -1 ||
                    cap_set_flag(m_cap, CAP_INHERITABLE, m_caps_list.size(), m_caps_list.data(), CAP_SET) == -1) {
                        throw std::runtime_error("Capabilities Manager Error: Failed to set caps flag.");
                }
        }

        if (cap_set_proc(m_cap) == -1) {
                throw std::runtime_error("Capabilities Manager Error: Failed to apply capabilities to process.");
        }

        for (std::size_t i{0}; i <= CAP_LAST_CAP; ++i) {
                cap_value_t cap_value{static_cast<cap_value_t>(i)};

                if (std::find(m_caps_list.begin(), m_caps_list.end(), cap_value) == m_caps_list.end()) {
                        if (prctl(PR_CAPBSET_DROP, cap_value, 0, 0, 0) == -1) {
                                throw std::runtime_error(std::format("Capabilities Manager Error: Failed to drop {} from bounding set.", i));
                        }
                }
        }
}

CapsManager::~CapsManager() {
        if (m_cap != nullptr) {
                cap_free(m_cap);
        }
}
