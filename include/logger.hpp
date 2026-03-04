#pragma once
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <filesystem>
namespace fs = std::filesystem;

class Logger{
        public:
                Logger();
                ~Logger();
                Logger(const Logger&) = delete;
                Logger(Logger&&) = delete;
                auto operator=(const Logger&) -> Logger& = delete;
                auto operator=(Logger&&) -> Logger& = delete;
                auto set_log_path(const fs::path& path) -> void;
                auto log(std::string buffer) -> void;
        private:
                std::thread m_worker{};
                std::queue<std::string> m_queue{};
                std::mutex m_mtx{};
                std::condition_variable m_cv{};
                std::atomic<bool> m_running{true};
                std::ofstream m_log_file{};
                fs::path m_log_path{};
};
