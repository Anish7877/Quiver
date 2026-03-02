#pragma once
#include "db_types.hpp"
#include "singleton.hpp"
#include <optional>
#include <string>

class DatabaseCommandQueue : public Singleton<DatabaseCommandQueue> {
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

                auto map_buffer(const std::string& buf_name, bool is_consumer) -> void;
                auto atomic_push(const JobData&) -> bool;
                auto atomic_pop() -> std::optional<JobData>;
                auto get_active_connections() const -> std::size_t;
                auto is_empty() const -> bool;
        private:
                JobSlot* m_mapped_address{nullptr};
                std::size_t m_buf_size{QUEUE_SIZE * sizeof(JobSlot)};
                std::string m_buf_name{};
                bool m_is_consumer{false};
                CommandQueueHeader *m_header{nullptr};
};
