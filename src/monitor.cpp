#include <iostream>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>
#include "utils.hpp"
#include "singleton.hpp"
#include "monitor.hpp"

Monitor::Monitor() {
        using namespace std::string_literals;
        m_worker = std::thread([this]() -> void {
                        while (true) {
                                std::string current_buffer;
                                fs::path current_path;
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
                                        if(m_log_path.empty()) [[unlikely]] {
                                                auto now{std::chrono::steady_clock::now()};
                                                m_log_path = "/tmp/"s + std::to_string(now.time_since_epoch().count()) + ".log"s;
                                                try { Utils::ensure_file(m_log_path); } catch (...) {}
                                        }
                                        current_path = m_log_path;

                                }
                                if (!current_path.empty()) {
                                        try {
                                                Utils::write_file(current_path, current_buffer);
                                        }
                                        catch(const std::exception& e) {
                                                std::cout << "Monitor write error due to:\n\t" << e.what() << '\n';
                                        }
                                }
                        }
        });
}

Monitor::~Monitor() {
        m_running.store(false);
        m_cv.notify_one();
        if(m_worker.joinable()) {
                m_worker.join();
        }
}

auto Monitor::set_log_path(const fs::path& log_path) -> void {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_log_path = log_path;
        try {
                Utils::ensure_file(log_path);
        }
        catch(const std::exception& e) {
                std::cout << e.what() << '\n';
        }
}

auto Monitor::log(std::string& buffer) -> void {
        {
                std::lock_guard<std::mutex> lock(m_mtx);
                m_queue.push(std::move(buffer));
        }
        m_cv.notify_one();
}
