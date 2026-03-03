#include "new_database_manager.hpp"
#include "db_types.hpp"
#include "utils.hpp"
#include <chrono>
#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#include <rocksdb/status.h>
#include <rocksdb/version.h>

auto DatabaseManager::init() -> void {
        m_container_db_path = Utils::get_container_db_path();
        m_volume_db_path = Utils::get_volume_db_path();
        m_device_db_path = Utils::get_device_db_path();
        m_network_db_path = Utils::get_network_db_path();
        m_image_db_path = Utils::get_image_db_path();
        rocksdb::Options options{};
        options.create_if_missing = true;
        rocksdb::Status status{rocksdb::DB::Open(options, m_container_db_path, &m_container_db)};
        if(!status.ok()) {
                throw std::runtime_error("Database Error: cannot open container database");
        }
        status = rocksdb::DB::Open(options, m_volume_db_path, &m_volume_db);
        if(!status.ok()) {
                throw std::runtime_error("Database Error: cannot open volume database");
        }
        status = rocksdb::DB::Open(options, m_device_db_path, &m_device_db);
        if(!status.ok()) {
                throw std::runtime_error("Database Error: cannot open device database");
        }
        status = rocksdb::DB::Open(options, m_network_db_path, &m_network_db);
        if(!status.ok()) {
                throw std::runtime_error("Database Error: cannot open network database");
        }
        status = rocksdb::DB::Open(options, m_image_db_path, &m_image_db);
        if(!status.ok()) {
                throw std::runtime_error("Database Error: cannot open image database");
        }
}

auto DatabaseManager::process_job(const JobData& job, const std::string& value, std::string& msg) -> void {
        switch (job.type) {
                case JobType::GET: process_get_job(job, msg); break;
                case JobType::PUT: process_put_job(job, value, msg); break;
                case JobType::UPDATE: process_update_job(job, value, msg); break;
                case JobType::DELETE: process_delete_job(job, msg); break;
                default: msg = std::format("[{}] Database Error: Unknown job type found.\n", std::chrono::high_resolution_clock::now());
        }
}

auto DatabaseManager::process_get_job(const JobData& job, std::string& msg) -> void {
        rocksdb::DB* db{nullptr};
        std::string db_string{};

        switch (job.target) {
                case TargetDB::CONTAINER: db_string = Utils::TargetDB_to_string(job.target); db = m_container_db; break;
                case TargetDB::VOLUME: db_string = Utils::TargetDB_to_string(job.target); db = m_volume_db; break;
                case TargetDB::DEVICE: db_string = Utils::TargetDB_to_string(job.target); db = m_device_db; break;
                case TargetDB::NETWORK: db_string = Utils::TargetDB_to_string(job.target); db = m_network_db; break;
                case TargetDB::IMAGE: db_string = Utils::TargetDB_to_string(job.target); db = m_image_db; break;
                default: msg = "Database Error: Invalid TargetDB routing.\n"; return;
        }

        if (db == nullptr) {
                msg = "Database Error: Invalid TargetDB routing.\n"; return;
        }

        rocksdb::Slice db_key{job.key};
        std::string fetched_data{};
        rocksdb::Status status{db->Get(rocksdb::ReadOptions(), db_key, &fetched_data)};

        if(status.IsNotFound()) {
                msg = std::format("[{}] {} Database Error: Key [{}] not found in database.\n",
                                        std::chrono::high_resolution_clock::now(),
                                        db_string, job.key);
        }
        else if(!status.ok()) {
                msg = std::format("[{}] {} Database Error: Read error -> {}.\n",
                                        std::chrono::high_resolution_clock::now(),
                                        db_string, status.ToString());
        }
        else {
                msg = std::move(fetched_data);
        }
}

auto DatabaseManager::process_put_job(const JobData& job, const std::string& value, std::string& msg) -> void {
        rocksdb::DB* db{nullptr};
        std::string db_string{};

        switch (job.target) {
                case TargetDB::CONTAINER: db_string = Utils::TargetDB_to_string(job.target); db = m_container_db; break;
                case TargetDB::VOLUME: db_string = Utils::TargetDB_to_string(job.target); db = m_volume_db; break;
                case TargetDB::DEVICE: db_string = Utils::TargetDB_to_string(job.target); db = m_device_db; break;
                case TargetDB::NETWORK: db_string = Utils::TargetDB_to_string(job.target); db = m_network_db; break;
                case TargetDB::IMAGE: db_string = Utils::TargetDB_to_string(job.target); db = m_image_db; break;
                default: msg = "Database Error: Invalid TargetDB routing.\n"; return;
        }

        if (db == nullptr) {
                msg = "Database Error: Invalid TargetDB routing.\n"; return;
        }

        rocksdb::Slice db_key{job.key};
        rocksdb::Status status{db->Put(rocksdb::WriteOptions(), db_key, value)};

        if(!status.ok()) {
                msg = std::format("[{}] {} Database Error: Write error -> {}.\n",
                                        std::chrono::high_resolution_clock::now(),
                                        db_string, status.ToString());
        }
        else {
                msg = std::format("[{}] {} Database: Successfully added or updated data.\n", std::chrono::high_resolution_clock::now(), db_string);
        }
}

auto DatabaseManager::process_update_job(const JobData& job, const std::string& value, std::string& msg) -> void {
        process_put_job(job, value, msg);
}

auto DatabaseManager::process_delete_job(const JobData& job, std::string& msg) -> void {
        rocksdb::DB* db{nullptr};
        std::string db_string{};

        switch (job.target) {
                case TargetDB::CONTAINER: db_string = Utils::TargetDB_to_string(job.target); db = m_container_db; break;
                case TargetDB::VOLUME: db_string = Utils::TargetDB_to_string(job.target); db = m_volume_db; break;
                case TargetDB::DEVICE: db_string = Utils::TargetDB_to_string(job.target); db = m_device_db; break;
                case TargetDB::NETWORK: db_string = Utils::TargetDB_to_string(job.target); db = m_network_db; break;
                case TargetDB::IMAGE: db_string = Utils::TargetDB_to_string(job.target); db = m_image_db; break;
                default: msg = "Database Error: Invalid TargetDB routing.\n"; return;
        }

        if (db == nullptr) {
                msg = "Database Error: Invalid TargetDB routing.\n"; return;
        }

        rocksdb::Slice db_key{job.key};
        rocksdb::Status status{db->Delete(rocksdb::WriteOptions(), db_key)};

        if(!status.ok()) {
                msg = std::format("[{}] {} Database Error: Delete error -> {}.\n",
                                        std::chrono::high_resolution_clock::now(),
                                        db_string, status.ToString());
        }
        else {
                msg = std::format("[{}] {} Database: Successfully deleted data.\n", std::chrono::high_resolution_clock::now(), db_string);
        }
}
