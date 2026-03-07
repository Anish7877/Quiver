#include "container_manager.hpp"
#include "utils.hpp"
#include "serialization.hpp"
#include <chrono>
#include <format>
namespace chrono = std::chrono;

auto ContainerManager::init() -> void {
        m_db_path = Utils::get_container_db_path();
        Utils::ensure_dir(m_db_path.parent_path());
        m_logger.set_log_path(Utils::get_container_db_log_path());
        rocksdb::Options options{};
        options.create_if_missing = true;
        rocksdb::Status status{rocksdb::DB::Open(options, m_db_path, &m_db)};
        if (!status.ok()) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] Containe Manager Error: could not open database.",
                                        chrono::high_resolution_clock::now()));
        }
}

auto ContainerManager::process_job(const JobData& job, const ContainerType& obj, Status& stat) -> void {
        stat.m_ok = false;
        switch (job.type) {
                case JobType::GET: process_get_job(job, stat); break;
                case JobType::PUT: process_put_job(job, obj, stat); break;
                case JobType::UPDATE: process_update_job(job, obj, stat); break;
                case JobType::DELETE: process_delete_job(job, stat); break;
                default:
                        m_logger.log(std::format("[{}] Container Manager Error: Unknown job type found.",
                                        chrono::high_resolution_clock::now()));
                        stat.m_error = "Container Manager Error: Unknown job type found.";
        }
}

auto ContainerManager::process_get_job(const JobData& job, Status& stat) -> void {
        if (m_db == nullptr) [[unlikely]] {
                m_logger.log(std::format("[{}] Container Manager Error: Manager not initialized.",
                                        chrono::high_resolution_clock::now()));
                stat.m_error = "Container Manager Error: Manager not initialized.";
                return;
        }
        rocksdb::Slice db_key{job.key};
        std::string fetched_data{};
        rocksdb::Status status{m_db->Get(rocksdb::ReadOptions(), db_key, &fetched_data)};

        if (status.IsNotFound()) [[unlikely]] {
                m_logger.log(std::format("[{}] Container Manager Error: Key [{}] not found in database.",
                                        chrono::high_resolution_clock::now(), job.key));
                stat.m_error = std::format("Container Manager Error: Key [{}] not found in database.", job.key);
        }
        else if (!status.ok()) [[unlikely]] {
                m_logger.log(std::format("[{}] Container Manager Error: Read error -> {}.",
                                        chrono::high_resolution_clock::now(), status.ToString()));
                stat.m_error = std::format("Container Manager Error: Read error -> {}.", status.ToString());
        }
        else {
                m_logger.log(std::format("[{}] Container Manager: Get job success.", chrono::high_resolution_clock::now()));
                stat.m_ok = true;
                stat.m_result = std::move(fetched_data);
        }
}

auto ContainerManager::process_put_job(const JobData& job, const ContainerType& obj, Status& stat) -> void {
        if (m_db == nullptr) [[unlikely]] {
                m_logger.log(std::format("[{}] Container Manager Error: Manager not initialized.",
                                        chrono::high_resolution_clock::now()));
                stat.m_error = "Container Manager Error: Manager not initialized.";
                return;
        }

        rocksdb::Slice db_key{job.key};
        std::string serialized_value{};
        rocksdb::Status status{m_db->Put(rocksdb::WriteOptions(), db_key, serialized_value)};

        if (!status.ok()) [[unlikely]] {
                m_logger.log(std::format("[{}] Container Manager Error: Write error -> {}.",
                                        chrono::high_resolution_clock::now(), status.ToString()));
                stat.m_error = std::format("Container Manager Error: Write error -> {}.", status.ToString());
        }
        else {
                m_logger.log(std::format("[{}] Container Manager: Put or Update job success.",
                                        chrono::high_resolution_clock::now()));
                stat.m_ok = true;
                stat.m_result = "Container Manager: Put or Update job success.";
        }
}

auto ContainerManager::process_update_job(const JobData& job, const ContainerType& obj, Status& stat) -> void {
        process_put_job(job, obj, stat);
}

auto ContainerManager::process_delete_job(const JobData& job, Status& stat) -> void {
        if (m_db == nullptr) [[unlikely]] {
                m_logger.log(std::format("[{}] Container Manager Error: Manager not initialized.",
                                        chrono::high_resolution_clock::now()));
                stat.m_error = "Container Manager Error: Manager not initialized.";
                return;
        }

        rocksdb::Slice db_key{job.key};
        rocksdb::Status status{m_db->Delete(rocksdb::WriteOptions(), db_key)};

        if (!status.ok()) [[unlikely]] {
                m_logger.log(std::format("[{}] Container Manager Error: Delete error -> {}.",
                                        chrono::high_resolution_clock::now(), status.ToString()));
                stat.m_error = std::format("[{}] Container Manager Error: Delete error -> {}.",
                                chrono::high_resolution_clock::now(), status.ToString());
        }
        else {
                m_logger.log(std::format("[{}] Container Manager: Delete job success.",
                                        chrono::high_resolution_clock::now()));
                stat.m_ok = true;
                stat.m_result = "Container Manager: Delete job success.";
        }
}

ContainerManager::~ContainerManager() {
        if (m_db != nullptr) {
                delete m_db;
        }
}
