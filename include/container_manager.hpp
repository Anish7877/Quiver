#pragma once
#include "types.hpp"
#include "new_database_manager.hpp"
#include "container_config.hpp"
#include "singleton.hpp"
#include <filesystem>
#include <rocksdb/db.h>
namespace fs = std::filesystem;

class LoggerCommandQueue;
class ValueHeap;
class ContainerManager : public DatabaseManager<ContainerType>, public Singleton<ContainerManager> {
        friend class Singleton<ContainerManager>;
        private:
                ContainerManager() = default;
                ~ContainerManager();
        public:
                ContainerManager(const ContainerManager&) = delete;
                ContainerManager(ContainerManager&&) = delete;
                auto operator=(const ContainerManager&) -> ContainerManager& = delete;
                auto operator=(ContainerManager&&) -> ContainerManager& = delete;

                auto init() -> void override;
                auto process_job(const DatabaseJobData&, const ContainerType&, Status&) -> void override;
                auto extract_container(const std::string&, Status&) -> ContainerType;
        private:
                auto process_get_job(const DatabaseJobData&, Status&) -> void override;
                auto process_put_job(const DatabaseJobData&, const ContainerType&, Status&) -> void override;
                auto process_update_job(const DatabaseJobData&, const ContainerType&, Status&) -> void override;
                auto process_delete_job(const DatabaseJobData&, Status&) -> void override;
                auto log_event(const std::string&) -> void;
                LogJobData m_log_job_data{};
                fs::path m_db_path{};
                rocksdb::DB* m_db{nullptr};
                LoggerCommandQueue* m_log_cmd_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
};
