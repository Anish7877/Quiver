#include <iostream>
#include <format>
#include "utils.hpp"
#include "logger.hpp"

Logger::Logger() {
        using namespace std::string_literals;
        m_worker = std::thread([this]() -> void {
                        while (true) {
                        std::string current_buffer{};
                        bool needs_path{false};
                        {
                                std::unique_lock<std::mutex> lock(m_mtx);
                                m_cv.wait(lock, [this]() -> bool {
                                                return !m_running.load() || !m_queue.empty();
                                        });
                                if (!m_running.load() && m_queue.empty()) {
                                        break;
                                }
                                current_buffer = std::move(m_queue.front());
                                m_queue.pop();
                                needs_path = m_log_path.empty();
                        }
                                if (needs_path) [[unlikely]] {
                                        auto now{std::chrono::steady_clock::now()};
                                        set_log_path("tmp/"s + std::to_string(now.time_since_epoch().count()) + ".log"s);
                                }
                                m_log_file << current_buffer << std::flush;
                        }
                });
}

auto Logger::set_log_path(const fs::path& log_path) -> void {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_log_path = log_path;
        try {
                Utils::ensure_file(log_path);
        }
        catch(const std::exception& e) {
                std::cout << e.what() << '\n';
        }
        m_log_file.open(m_log_path, std::ios::app);
        if(!m_log_file.is_open()) [[unlikely]] {
                throw std::runtime_error(std::format("Logger Error: couldn't open '{}'", m_log_path.string()));
        }
}

auto Logger::log(std::string buffer) -> void {
        {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_queue.push(std::move(buffer));
        }
        m_cv.notify_one();
}

Logger::~Logger() {
        m_running.store(false);
        m_cv.notify_all();
        if(m_worker.joinable()) {
                m_worker.join();
        }
}
