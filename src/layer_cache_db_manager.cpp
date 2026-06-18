#include "layer_cache_db_manager.hpp"
#include "layer_cache_generated.h"
#include "serialization.hpp"
#include "types.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
namespace chrono = std::chrono;

auto LayerCacheDbManager::init() -> void {
        rocksdb::Options opts{};
        opts.create_if_missing = true;
        rocksdb::Status status{rocksdb::DB::Open(opts, m_db_path, &m_db)};
        if (!status.ok()) [[unlikely]] {
                throw std::runtime_error(std::format("[{}] Layer Cache DB Manager Error: Could not open Database.",
                                        chrono::system_clock::now()));
        }
}

auto LayerCacheDbManager::process_job(const DatabaseJobData& job_data, const LayerCache& obj) -> void {
        job_data.status->m_ok = false;
        switch (job_data.type) {
                case JobType::GET:
                        process_get_job(job_data); break;
                case JobType::PUT:
                        process_put_job(job_data, obj); break;
                case JobType::UPDATE:
                        process_update_job(job_data, obj); break;
                case JobType::DELETE:
                        process_delete_job(job_data); break;
                case JobType::GETALL:
                        process_get_all_job(job_data); break;
                default:
                       job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: Unknown job type found.",
                                        chrono::system_clock::now()));
                       job_data.status->processed.store(true, std::memory_order_release);
                       job_data.status->processed.notify_all();
        }
}

auto LayerCacheDbManager::extract_obj(const std::string& raw_bytes) -> std::optional<LayerCache> {
        flatbuffers::Verifier verifier{reinterpret_cast<const uint8_t*>(raw_bytes.data()), raw_bytes.size()};
        if (!verifier.VerifyBuffer<FB::LayerCache>(nullptr)) {
                return std::nullopt;
        }
        const auto* fb_root{flatbuffers::GetRoot<FB::LayerCache>(raw_bytes.data())};
        LayerCache obj{Serialization::deserialize(fb_root)};
        return obj;
}

auto LayerCacheDbManager::process_get_job(const DatabaseJobData& job_data) -> void {
        if (m_db == nullptr) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: manager not initialized.",
                                        chrono::system_clock::now()));
                return;
        }
        rocksdb::Slice key{job_data.key};
        std::string raw_bytes{};
        rocksdb::Status stat{m_db->Get(rocksdb::ReadOptions(), key, &raw_bytes)};
        if (stat.IsNotFound()) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: Key not found.",
                                        chrono::system_clock::now()));
        }
        else if (!stat.ok()) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: Read error -> '{}'.",
                                        chrono::system_clock::now(), stat.ToString()));
        }
        else {
               job_data.status->m_ok = true;
               job_data.status->m_results.emplace_back(std::move(job_data.key), std::move(raw_bytes));
        }
        job_data.status->processed.store(true, std::memory_order_release);
        job_data.status->processed.notify_all();
}

auto LayerCacheDbManager::process_put_job(const DatabaseJobData& job_data, const LayerCache& obj) -> void {
        if (m_db == nullptr) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: manager not initialized.",
                                        chrono::system_clock::now()));
                return;
        }
        rocksdb::Slice key{job_data.key};
        flatbuffers::FlatBufferBuilder builder{};
        flatbuffers::Offset<FB::LayerCache> fb_offset{Serialization::serialize(builder, obj)};
        builder.Finish(fb_offset);
        std::string raw_bytes{reinterpret_cast<const char*>(builder.GetBufferPointer(), builder.GetSize())};
        rocksdb::Status stat{m_db->Put(rocksdb::WriteOptions(), key, raw_bytes)};
        if (!stat.ok()) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: Write error -> '{}'.",
                                        chrono::system_clock::now(), stat.ToString()));
        }
        else {
               job_data.status->m_ok = true;
        }
        job_data.status->processed.store(true, std::memory_order_release);
        job_data.status->processed.notify_all();
}

auto LayerCacheDbManager::process_update_job(const DatabaseJobData& job_data, const LayerCache& obj) -> void {
        process_put_job(job_data, obj);
}

auto LayerCacheDbManager::process_delete_job(const DatabaseJobData& job_data) -> void {
        if (m_db == nullptr) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: manager not initialized.",
                                        chrono::system_clock::now()));
                return;
        }
        rocksdb::Slice key{job_data.key};
        rocksdb::Status stat{m_db->Delete(rocksdb::WriteOptions(), key)};
        if (!stat.ok()) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: Write error -> '{}'.",
                                        chrono::system_clock::now(), stat.ToString()));
        }
        else {
               job_data.status->m_ok = true;
        }
        job_data.status->processed.store(true, std::memory_order_release);
        job_data.status->processed.notify_all();
}

auto LayerCacheDbManager::process_get_all_job(const DatabaseJobData& job_data) -> void {
        if (m_db == nullptr) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: manager not initialized.",
                                        chrono::system_clock::now()));
                return;
        }
        auto it{m_db->NewIterator(rocksdb::ReadOptions())};
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
               job_data.status->m_results.emplace_back(std::move(it->key().ToString()), std::move(it->value().ToString()));
        }
        if (!it->status().ok()) [[unlikely]] {
               job_data.status->m_error = std::move(std::format("[{}] Layer Cache DB Manager Error: Read Error -> '{}'.",
                                        chrono::system_clock::now(), it->status().ToString()));
        }
        else {
               job_data.status->m_ok = true;
        }
        job_data.status->processed.store(true, std::memory_order_release);
        job_data.status->processed.notify_all();
}
