#pragma once
#include "db_types.hpp"
#include "singleton.hpp"
#include <string>
#include <rocksdb/status.h>
#include <rocksdb/db.h>

class DatabaseManager {
        private:
        public:
                DatabaseManager() = default;
                ~DatabaseManager() = default;
                DatabaseManager(const DatabaseManager&) = delete;
                DatabaseManager(DatabaseManager&&) = delete;
                auto operator=(const DatabaseManager&) = delete;
                auto operator=(DatabaseManager&&) = delete;
                auto init() -> void;
                auto process_job(const JobData&, const std::string&, std::string&) -> void;
        private:
                auto process_get_job(const JobData&, std::string&) -> void;
                auto process_put_job(const JobData&, const std::string&, std::string&) -> void;
                auto process_update_job(const JobData&, const std::string&, std::string&) -> void;
                auto process_delete_job(const JobData&, std::string&) -> void;
                std::string m_container_db_path{};
                std::string m_volume_db_path{};
                std::string m_device_db_path{};
                std::string m_network_db_path{};
                std::string m_image_db_path{};
                rocksdb::DB* m_container_db{};
                rocksdb::DB* m_volume_db{};
                rocksdb::DB* m_device_db{};
                rocksdb::DB* m_network_db{};
                rocksdb::DB* m_image_db{};
};
