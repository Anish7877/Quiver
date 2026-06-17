#pragma once
#include "types.hpp"
#include "container_config.hpp"
#include "new_database_manager.hpp"
#include "singleton.hpp"
#include <filesystem>
#include <rocksdb/db.h>
namespace fs = std::filesystem;

class LoggerCommandQueue;
class ValueHeap;
class ContainerDbManager : public DatabaseManager<ContainerConfig>, public Singleton<ContainerDbManager> {
        friend class Singleton<ContainerDbManager>;
        private:
                ContainerDbManager() = default;
                ~ContainerDbManager() = default;
        public:
                ContainerDbManager(const ContainerDbManager&) = delete;
                ContainerDbManager(ContainerDbManager&&) = delete;
                auto operator=(const ContainerDbManager&) -> ContainerDbManager& = delete;
                auto operator=(ContainerDbManager&&) -> ContainerDbManager& = delete;

                auto init() -> void override;
                auto process_job(const DatabaseJobData&, const ContainerConfig&, Status&) -> void override;
                auto extract_container(const std::string&, Status&) -> ContainerConfig;
        private:
                auto process_get_job(const DatabaseJobData&, Status&) -> void override;
                auto process_put_job(const DatabaseJobData&, const ContainerConfig&, Status&) -> void override;
                auto process_update_job(const DatabaseJobData&, const ContainerConfig&, Status&) -> void override;
                auto process_delete_job(const DatabaseJobData&, Status&) -> void override;
                auto log_event(const std::string&) -> void;
                LogJobData m_log_job_data{};
                fs::path m_db_path{};
                rocksdb::DB* m_db{nullptr};
                LoggerCommandQueue* m_log_cmd_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
};
