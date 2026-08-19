#include "thread_pool.hpp"
#include <functional>
#include <mutex>

ThreadPool::ThreadPool(size_t n_workers) {
        m_workers.resize(n_workers);
        for (auto& worker : m_workers) {
                worker = std::jthread([this]() {
                                        while (true) {
                                                std::function<void()> task{};
                                                {
                                                        std::unique_lock lock(m_mutex);
                                                        m_cv.wait(lock, [&]() {
                                                                                return m_stop || !m_tasks.empty();
                                                                        });
                                                        if (m_stop && m_tasks.empty()) {
                                                                return;
                                                        }
                                                        task = std::move(m_tasks.front());
                                                        m_tasks.pop();
                                                }
                                                task();
                                        }
                                });
        }
}

ThreadPool::~ThreadPool() {
        {
                std::lock_guard lock(m_mutex);
                m_stop = true;
        }
        m_cv.notify_all();
        for (auto& worker : m_workers) {
                if (worker.joinable()) worker.join();
        }
}
