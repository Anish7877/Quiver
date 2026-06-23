#include "database_command_queue.hpp"
#include "layer_cache_manager.hpp"
#include "layer_cache_db_manager.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value_heap.hpp"
#include <cstring>
#include <flatbuffers/verifier.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

[[nodiscard]] auto InFlightCacheManager::acquire(const std::string& hash) -> AcquireResult {
        std::shared_ptr<InFlightBuild> build{};
        if (m_inflight.find(hash, build)) {
                return {false, std::move(build)};
        }
        build = std::make_shared<InFlightBuild>();
        if (m_inflight.insert(hash, build)) {
                return {true, std::move(build)};
        }
        m_inflight.find(hash, build);
        return {false, std::move(build)};
}

auto InFlightCacheManager::finish_success(const std::string& hash, LayerCache&& layer) -> void {
        std::shared_ptr<InFlightBuild> build{};
        if (!m_inflight.find(hash, build)) {
                throw std::runtime_error("In Flight Cache Manager Error: Hash not found.");
        }
        build->promise.set_value(std::move(layer));
        m_inflight.erase(hash);
}

auto InFlightCacheManager::finish_failure(const std::string& hash, std::exception_ptr eptr) -> void {
        std::shared_ptr<InFlightBuild> build{};
        if (!m_inflight.find(hash, build)) {
                throw std::runtime_error("In Flight Cache Manager Error: Hash not found.");
        }
        build->promise.set_exception(eptr);
        m_inflight.erase(hash);
}

auto LayerCacheManager::init() -> void {
        m_db_command_queue = &DatabaseCommandQueue::get_instance();
        m_value_heap = &ValueHeap::get_instance();
        m_value_heap->map_buffer(Utils::get_value_heap_buf_name(), ValueHeap::VALUE_HEAP_SIZE, false);
        if (!m_value_heap->ok()) [[unlikely]] {
                throw std::runtime_error(m_value_heap->get_error());
        }
        m_db_command_queue->map_buffer(Utils::get_database_command_queue_buf_name(), false);
        if (!m_db_command_queue->ok()) {
                throw std::runtime_error(m_db_command_queue->get_error());
        }
}

[[nodiscard]] auto LayerCacheManager::lookup(const std::string& key) -> std::optional<LayerCache> {
        DatabaseJobData job_data{};
        DbStatus status{};
        job_data.target = TargetDB::LAYERCACHE;
        job_data.type = JobType::GET;
        job_data.status = &status;
        std::memcpy(job_data.key, key.data(), 32);
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
        status.wait();
        if (!status.ok()) {
                throw std::runtime_error(status.get_error());
        }
        const auto& results{status.get_result()};
        return LayerCacheDbManager::extract_obj(results.front().value);
}

auto LayerCacheManager::store(const std::string& key, const std::string& value) -> void {
        LayerCache obj{value};
        flatbuffers::FlatBufferBuilder builder{};
        flatbuffers::Offset<FB::LayerCache> fb_offset{Serialization::serialize(builder, obj)};
        builder.Finish(fb_offset);
        std::string raw_bytes{reinterpret_cast<const char*>(builder.GetBufferPointer(), builder.GetSize())};
        DatabaseJobData job_data{};
        DbStatus status{};
        job_data.target = TargetDB::LAYERCACHE;
        job_data.type = JobType::PUT;
        job_data.status = &status;
        job_data.value_length = raw_bytes.length();
        std::memcpy(job_data.key, key.data(), 32);
        while (!m_value_heap->write_job_data(raw_bytes, job_data.value_offset)) {
                std::this_thread::yield();
        }
        while (!m_db_command_queue->atomic_push(job_data)) {
                std::this_thread::yield();
        }
}
