#pragma once
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include "singleton.hpp"

class ContainerMonitor : public Singleton<ContainerMonitor> {
        friend class Singleton<ContainerMonitor>;
        private:
                ContainerMonitor();
                ~ContainerMonitor();
        public:
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
