#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <thread>

class ThreadPool {
        public:
                explicit ThreadPool(size_t);
                ~ThreadPool();
                ThreadPool(const ThreadPool&) = delete;
                ThreadPool(ThreadPool&&) = delete;
                auto operator=(const ThreadPool&) -> ThreadPool& = delete;
                auto operator=(ThreadPool&&) -> ThreadPool& = delete;

                template <typename Func, typename... Args>
                auto submit(Func&& f, Args&&... args) {
                        using ResultType = std::invoke_result_t<Func, Args...>;
                        auto task{std::make_shared<std::packaged_task<ResultType()>>(
                                        std::bind(std::forward<Func>(f),std::forward<Args>(args)...))};
                        auto future{task->get_future()};
                        {
                                std::lock_guard lock(m_mutex);
                                if (m_stop) {
                                        throw std::runtime_error("Thread Pool is stopping");
                                }
                                m_tasks.emplace([task]() {
                                                        (*task)();
                                                });
                        }
                        m_cv.notify_one();
                        return future;
                }
        private:
                std::queue<std::function<void()>> m_tasks{};
                std::mutex m_mutex{};
                std::condition_variable m_cv{};
                std::vector<std::thread> m_workers{};
                bool m_stop{false};
};
