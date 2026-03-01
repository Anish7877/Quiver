#pragma once
#include "db_job_types.hpp"
#include "singleton.hpp"
#include <optional>
#include <string>
#include <atomic>

class DatabaseCommandQueue : public Singleton<DatabaseCommandQueue> {
        friend class Singleton<DatabaseCommandQueue>;
        private:
                DatabaseCommandQueue() = default;
                ~DatabaseCommandQueue() = default;
        public:
                DatabaseCommandQueue(const DatabaseCommandQueue&) = delete;
                DatabaseCommandQueue(DatabaseCommandQueue&&) = delete;
                auto operator=(const DatabaseCommandQueue&) -> DatabaseCommandQueue& = delete;
                auto operator=(DatabaseCommandQueue&&) -> DatabaseCommandQueue& = delete;

                auto map_buffer(const std::string& buf_name, bool is_consumer) -> void;
                auto atomic_push(const JobData&) -> bool;
                auto atomic_pop() -> std::optional<JobData>;
        private:
                void* m_mapped_address{nullptr};
                std::size_t m_buf_size{sizeof(RingBuffer<JobSlot>)};
                std::string m_buf_name{};
                bool m_is_consumer{false};
};
