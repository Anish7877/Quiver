#pragma once
#include "job_processor.hpp"
#include "singleton.hpp"
#include "types.hpp"
#include <memory>
#include <rocksdb/db.h>
#include <thread>
#include <atomic>

class DatabaseCommandQueue;
class LoggerCommandQueue;
class ValueHeap;
class DatabaseJobProcessor : public JobProcessor<DatabaseJobData>, public Singleton<DatabaseJobProcessor> {
        friend class Singleton<DatabaseJobProcessor>;
        private:
                DatabaseJobProcessor() = default;
                ~DatabaseJobProcessor() = default;
        public:
                DatabaseJobProcessor(const DatabaseJobProcessor&) = delete;
                DatabaseJobProcessor(DatabaseJobProcessor&&) = delete;
                auto operator=(const DatabaseJobProcessor&) -> DatabaseJobProcessor& = delete;
                auto operator=(DatabaseJobProcessor&&) -> DatabaseJobProcessor& = delete;

                auto init() -> void override;
                auto process_job() -> void override;
        private:
                auto route_job(const DatabaseJobData&) -> void override;
                auto process_get_job(const DatabaseJobData&) -> void;
                auto process_put_job(const DatabaseJobData&) -> void;
                auto process_update_job(const DatabaseJobData&) -> void;
                auto process_delete_job(const DatabaseJobData&) -> void;
                auto process_get_all_job(const DatabaseJobData&) -> void;
                auto stop() -> void;
                auto log_event(const std::string&) -> void;
                std::unique_ptr<rocksdb::DB>* m_current_db{};
                std::unique_ptr<rocksdb::DB> m_container_db{};
                std::unique_ptr<rocksdb::DB> m_image_db{};
                std::unique_ptr<rocksdb::DB> m_layer_cache_db{};
                DatabaseCommandQueue* m_db_command_queue{};
                ValueHeap* m_value_heap{};
                LoggerCommandQueue* m_log_command_queue{};
                LogJobData m_log_data{};
                std::atomic<bool> m_running{};
                std::thread m_worker{};
};
