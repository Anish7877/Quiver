#pragma once
#include "types.hpp"
#include "singleton.hpp"
#include "command_queue.hpp"
#include <optional>
#include <string>

class DatabaseCommandQueue : public CommandQueue<DatabaseJobData>, public Singleton<DatabaseCommandQueue> {
        friend class Singleton<DatabaseCommandQueue>;
        static constexpr std::size_t QUEUE_SIZE{25000};
        private:
                DatabaseCommandQueue() = default;
                ~DatabaseCommandQueue();
        public:
                DatabaseCommandQueue(const DatabaseCommandQueue&) = delete;
                DatabaseCommandQueue(DatabaseCommandQueue&&) = delete;
                auto operator=(const DatabaseCommandQueue&) -> DatabaseCommandQueue& = delete;
                auto operator=(DatabaseCommandQueue&&) -> DatabaseCommandQueue& = delete;

                auto map_buffer(const std::string&, bool) -> void override;
                auto atomic_push(const DatabaseJobData&) -> bool override;
                auto atomic_pop() -> std::optional<DatabaseJobData> override;
                auto get_active_connections() const -> std::size_t override;
                auto is_empty() const -> bool override;
                auto ok() const -> bool { return m_ok; }
                auto get_error() const -> std::string { return m_error; }
        private:
                std::string m_buf_name{};
                std::string m_error{};
                JobSlot* m_mapped_address{nullptr};
                CommandQueueHeader *m_header{nullptr};
                std::size_t m_buf_size{QUEUE_SIZE * sizeof(JobSlot)};
                bool m_ok{true};
                bool m_is_consumer{false};
};
