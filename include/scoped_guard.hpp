#pragma once
#include <functional>
#include <utility>

class ScopeGuard {
        public:
                explicit ScopeGuard(std::function<void()> cleanup) : m_cleanup{std::move(cleanup)} {}
                ~ScopeGuard() {
                        if (m_active && m_cleanup) m_cleanup();
                }
                ScopeGuard(const ScopeGuard&) = delete;
                ScopeGuard& operator=(const ScopeGuard&) = delete;
                ScopeGuard(ScopeGuard&&) = delete;
                ScopeGuard& operator=(ScopeGuard&&) = delete;

                auto dismiss() -> void { m_active = false; }
        private:
                std::function<void()> m_cleanup;
                bool m_active{true};
};
