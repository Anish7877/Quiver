#pragma once
#include "types.hpp"
#include "logger.hpp"
#include "new_database_manager.hpp"
#include "singleton.hpp"
#include <filesystem>
#include <rocksdb/db.h>
namespace fs = std::filesystem;

class ContainerManager : public DatabaseManager, public Singleton<ContainerManager> {
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
                auto process_job(const JobData&, const std::string& value, Status&) -> void override;
        private:
                auto process_get_job(const JobData&, Status&) -> void override;
                auto process_put_job(const JobData&, const std::string&, Status&) -> void override;
                auto process_update_job(const JobData&, const std::string&, Status&) -> void override;
                auto process_delete_job(const JobData&, Status&) -> void override;
                Logger m_logger{};
                fs::path m_db_path{};
                rocksdb::DB* m_db{nullptr};
};
