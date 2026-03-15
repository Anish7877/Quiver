#include <atomic>
#include <iostream>
#include <format>
#include "utils.hpp"
#include "logger.hpp"

auto Logger::set_log_path(const fs::path& log_path) -> void {
        std::lock_guard<std::mutex> lock{m_mtx};
        m_log_path = log_path;
        try {
                Utils::ensure_file(log_path);
        }
        catch(const std::exception& e) {
                std::cerr << e.what() << '\n';
        }
        m_log_file.open(m_log_path, std::ios::app);
        if(!m_log_file.is_open()) [[unlikely]] {
                throw std::runtime_error(std::format("Logger Error: couldn't open '{}'", m_log_path.string()));
        }

        m_worker = std::thread([this]() -> void {
                        while (true) {
                        std::string current_buffer{};
                        {
                                std::unique_lock<std::mutex> lock(m_mtx);
                                m_cv.wait(lock, [this]() -> bool {
                                                return !m_running.load(std::memory_order_acquire) || !m_queue.empty();
                                                });
                                if (!m_running.load(std::memory_order_acquire) && m_queue.empty()) break;

                                current_buffer = std::move(m_queue.front());
                                m_queue.pop();
                        }
                                m_log_file << current_buffer << '\n' << std::flush;
                        }
                });
}

auto Logger::log(std::string buffer) -> void {
        {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_queue.push(std::move(buffer));
        }
        m_cv.notify_one();
}

Logger::~Logger() {
        m_running.store(false, std::memory_order_release);
        m_cv.notify_all();
        if(m_worker.joinable()) {
                m_worker.join();
        }
}
