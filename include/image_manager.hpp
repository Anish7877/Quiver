#pragma once
#include "types.hpp"
#include "singleton.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <future>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

// Forward declarations — headers commented out until those subsystems are re-enabled
class LoggerCommandQueue;
class ValueHeap;

class ImageManager : public Singleton<ImageManager> {
    friend class Singleton<ImageManager>;

private:
    ImageManager() = default;
    ~ImageManager();

public:
    ImageManager(const ImageManager&)                    = delete;
    ImageManager(ImageManager&&)                         = delete;
    auto operator=(const ImageManager&) -> ImageManager& = delete;
    auto operator=(ImageManager&&)      -> ImageManager& = delete;

    // Interface
    auto init()  -> void;
    auto pull(const std::string& image_name, std::string& out_path, std::string& error) -> json;
    auto remove(const std::string& image_name, std::string& error) -> bool;

    // --- Commented out: DatabaseManager base and job queue dispatch ---
    // auto process_job(const DatabaseJobData& job_data, const ImageType& obj, Status& stat) -> void override;

private:
    // --- Commented out: RocksDB job handlers (re-enable with DB subsystem) ---
    // auto process_get_job   (const DatabaseJobData& job_data, Status& stat)                        -> void;
    // auto process_put_job   (const DatabaseJobData& job_data, const ImageType& obj, Status& stat)  -> void;
    // auto process_update_job(const DatabaseJobData& job_data, const ImageType& obj, Status& stat)  -> void;
    // auto process_delete_job(const DatabaseJobData& job_data, Status& stat)                        -> void;

    // Registry & Layer Management
    auto get_auth_token  (const std::string& repo, std::string& out_token, std::string& error) -> bool;
    
    // Updated signature: returns media_type to handle multi-arch lists
    auto fetch_manifest  (const std::string& repo, const std::string& tag,
                          const std::string& token, json& out_manifest, 
                          std::string& out_media_type, std::string& error) -> bool;

    auto fetch_config_blob(const std::string& repo, const std::string& digest,
                           const std::string& token, json& out_config,
                           std::string& error) -> bool;

    auto download_layer(const std::string& repo, const std::string& digest,
                        const std::string& token, const fs::path& dest,
                        std::size_t expected_size) -> std::string;

    auto extract_layer(const fs::path& tarball_path, const fs::path& destination,
                       std::string& error) -> bool;

    // Utility
    auto extract_image_meta(const std::string& image_name,
                            std::string& repo, std::string& tag) -> void;

    // --- Commented out: logger subsystem (re-enable with LoggerCommandQueue/ValueHeap) ---
    // auto log_event(const std::string& log_data) -> void;

    // --- Commented out: RocksDB handle (re-enable with DB subsystem) ---
    // rocksdb::DB* m_db{nullptr};

    fs::path m_db_path{};
    fs::path m_images_root{};
    std::string baseCachePath{};

    // Retained as nullptr — safe to leave declared while subsystems are disabled
    LoggerCommandQueue* m_log_cmd_queue{nullptr};
    ValueHeap* m_value_heap{nullptr};

    // --- Commented out: only needed by log_event ---
    // LogJobData m_log_job_data{};
};
