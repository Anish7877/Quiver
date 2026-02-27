#pragma once

class Monitor : public Singleton<Monitor> {
        friend class Singleton<Monitor>;
        private:
                Monitor();
                ~Monitor();
        public:
                auto set_log_path(const fs::path& path) -> void;
                auto log(std::string& buffer) -> void;
        private:
                std::thread m_worker{};
                std::queue<std::string> m_queue{};
                std::mutex m_mtx{};
                std::condition_variable m_cv{};
                std::atomic<bool> m_running{};
                fs::path m_log_path{};
};
