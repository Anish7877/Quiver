#pragma once
#include "command_queue.hpp"
#include "singleton.hpp"
#include "types.hpp"

class LoggerCommandQueue : public CommandQueue<LogJobData>, public Singleton<LoggerCommandQueue> {
        friend class Singleton<LoggerCommandQueue>;
        static constexpr std::size_t QUEUE_SIZE{25000};
        private:
                LoggerCommandQueue() = default;
                ~LoggerCommandQueue();
        public:
                LoggerCommandQueue(const LoggerCommandQueue&) = delete;
                LoggerCommandQueue(LoggerCommandQueue&&) = delete;
                auto operator=(const LoggerCommandQueue&) -> LoggerCommandQueue& = delete;
                auto operator=(LoggerCommandQueue&&) -> LoggerCommandQueue& = delete;

                auto map_buffer(const std::string&, bool) -> void override;
                auto atomic_push(const LogJobData&) -> bool override;
                auto atomic_pop() -> std::optional<LogJobData> override;
                auto get_active_connections() const -> std::size_t override;
                auto is_empty() const -> bool override;
                auto ok() const -> bool { return m_ok; };
                auto get_error() const -> std::string { return m_error; };
        private:
                std::string m_bufname{};
                std::string m_error{};
                LogSlot* m_mapped_address{nullptr};
                CommandQueueHeader* m_header{nullptr};
                std::size_t m_buf_size{QUEUE_SIZE * sizeof(LogSlot)};
                bool m_ok{true};
                bool m_is_consumer{false};
};
