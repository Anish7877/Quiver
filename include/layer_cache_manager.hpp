#pragma once
#include "singleton.hpp"
#include "types.hpp"
#include <libcuckoo/cuckoohash_map.hh>
#include <optional>
#include <memory>
#include <future>

class DatabaseCommandQueue;
class ValueHeap;

class InFlightCacheManager : public Singleton<InFlightCacheManager>{
        friend class Singleton<InFlightCacheManager>;
        private:
                InFlightCacheManager() = default;
                ~InFlightCacheManager() = default;
        public:
                struct InFlightBuild {
                        InFlightBuild() : future{promise.get_future().share()} {}
                        std::promise<LayerCache> promise{};
                        std::shared_future<LayerCache> future{};
                };
                InFlightCacheManager(InFlightCacheManager&&) = delete;
                InFlightCacheManager(const InFlightCacheManager&) = delete;
                auto operator=(InFlightCacheManager&&) -> InFlightCacheManager& = delete;
                auto operator=(const InFlightCacheManager&) -> InFlightCacheManager& = delete;
        private:
                libcuckoo::cuckoohash_map<std::string, std::shared_ptr<InFlightBuild>> m_inflight{};
};

class LayerCacheManager {
        public:
                LayerCacheManager() = default;
                ~LayerCacheManager() = default;
                LayerCacheManager(LayerCacheManager&&) = delete;
                LayerCacheManager(const LayerCacheManager&) = delete;
                auto operator=(LayerCacheManager&&) -> LayerCacheManager& = delete;
                auto operator=(const LayerCacheManager&) -> LayerCacheManager& = delete;

                auto init() -> void;
                [[nodiscard]] auto lookup(const std::string&) -> std::optional<LayerCache>;
                auto store(const std::string&, const std::string&) -> void;
        private:
                std::unique_ptr<DatabaseCommandQueue> m_db_command_queue{};
                std::unique_ptr<ValueHeap> m_value_heap{};
};
