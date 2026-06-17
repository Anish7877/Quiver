#pragma once
#include "new_database_manager.hpp"
#include "singleton.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <memory>
#include <rocksdb/db.h>

class DatabaseCommandQueue;
class ValueHeap;
class LayerCacheDbManager : DatabaseManager<LayerCache>, public Singleton<LayerCacheDbManager>{
        friend class Singleton<LayerCacheDbManager>;
        private:
                LayerCacheDbManager() = default;
                ~LayerCacheDbManager();
        public:
                LayerCacheDbManager(const LayerCacheDbManager&) = delete;
                LayerCacheDbManager(LayerCacheDbManager&&) = delete;
                auto operator=(LayerCacheDbManager&&) -> LayerCache& = delete;
                auto operator=(const LayerCacheDbManager&) -> LayerCache& = delete;

                auto init() -> void override;
                auto process_job(const DatabaseJobData&, const LayerCache&, DbStatus&) -> void override;
                auto extract_obj(const std::string&) -> std::optional<LayerCache> override;
        private:
                auto process_get_job(const DatabaseJobData&, DbStatus&) -> void override;
                auto process_put_job(const DatabaseJobData&, const LayerCache&, DbStatus&) -> void override;
                auto process_update_job(const DatabaseJobData&, const LayerCache&, DbStatus&) -> void override;
                auto process_delete_job(const DatabaseJobData&, DbStatus&) -> void override;
                auto process_get_all_job(const DatabaseJobData&, DbStatus&) -> void override;
                std::filesystem::path m_db_path{Utils::get_db_path("layer_cache_db")};
                std::unique_ptr<rocksdb::DB> m_db{nullptr};
};
