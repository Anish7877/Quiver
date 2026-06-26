#pragma once
#include "singleton.hpp"
#include "types.hpp"
#include <libcuckoo/cuckoohash_map.hh>
#include <optional>
#include <memory>
#include <future>
#include <filesystem>
namespace fs = std::filesystem;

class DatabaseCommandQueue;
class ValueHeap;
class InFlightCacheManager : public Singleton<InFlightCacheManager> {
        friend class Singleton<InFlightCacheManager>;
        private:
                InFlightCacheManager() = default;
                ~InFlightCacheManager() = default;
        public:
                struct InFlightBuild {
                        InFlightBuild() : future{promise.get_future().share()} {}
                        std::promise<std::pair<LayerCache, fs::path>> promise{};
                        std::shared_future<std::pair<LayerCache, fs::path>> future{};
                };
                struct AcquireResult {
                        bool is_owner{};
                        std::shared_ptr<InFlightBuild> build{};
                };
                InFlightCacheManager(InFlightCacheManager&&) = delete;
                InFlightCacheManager(const InFlightCacheManager&) = delete;
                auto operator=(InFlightCacheManager&&) -> InFlightCacheManager& = delete;
                auto operator=(const InFlightCacheManager&) -> InFlightCacheManager& = delete;

                [[nodiscard]] auto acquire(const std::string&) -> AcquireResult;
                auto finish_success(const std::string&, const std::pair<LayerCache, fs::path>&) -> void;
                auto finish_failure(const std::string&, std::exception_ptr) -> void;
        private:
                libcuckoo::cuckoohash_map<std::string, std::shared_ptr<InFlightBuild>> m_inflight{};
};

class LayerCacheManager : public Singleton<LayerCacheManager>{
        friend class Singleton<LayerCacheManager>;
        private:
                LayerCacheManager() = default;
                ~LayerCacheManager() = default;
        public:
                LayerCacheManager(LayerCacheManager&&) = delete;
                LayerCacheManager(const LayerCacheManager&) = delete;
                auto operator=(LayerCacheManager&&) -> LayerCacheManager& = delete;
                auto operator=(const LayerCacheManager&) -> LayerCacheManager& = delete;

                auto init() -> void;
                [[nodiscard]] auto lookup(const std::string&) -> std::optional<LayerCache>;
                auto store(const std::string&, const std::string&) -> void;
        private:
                DatabaseCommandQueue* m_db_command_queue{};
                ValueHeap* m_value_heap{};
};
