#pragma once
#include "container_config.hpp"
#include <cstdint>
#include <atomic>
#include <string>
#include <new>
#include <vector>

struct DbStatus;
enum class JobType : std::uint8_t {
        GET = 0,
        PUT = 1,
        UPDATE = 2,
        DELETE = 3,
        GETALL = 4
};

enum class SlotState : std::uint32_t {
        EMPTY = 0,
        WRITING = 1,
        READY = 2
};

enum class TargetDB : std::uint8_t {
        CONTAINER = 0,
        IMAGE = 2,
        LAYERCACHE = 3
};

enum class TargetLog : std::uint8_t {
        DBLOG = 0,
        CONTAINERLOG = 1,
        CONTAINERMON = 2
};

struct alignas(std::hardware_destructive_interference_size) JobSlot {
        std::atomic<SlotState> state{};
        char key[32]{};
        JobType type{};
        TargetDB target{};
        std::uint64_t value_offset{};
        std::uint64_t value_length{};
};

struct alignas(std::hardware_destructive_interference_size) LogSlot {
        std::atomic<SlotState> state{};
        TargetLog target_log{};
        std::uint64_t value_offset{};
        std::uint64_t value_length{};
};

struct DatabaseJobData {
        DbStatus* status{nullptr};
        char key[32]{};
        JobType type{};
        TargetDB target{};
        std::uint64_t value_offset{};
        std::uint64_t value_length{};
};

struct LogJobData {
        TargetLog target_log{};
        std::uint64_t value_offset{};
        std::uint64_t value_length{};
};

struct CommandQueueHeader {
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> head{0};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> tail{0};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> connections{0};
};

struct ValueHeapHeader {
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> data_head{0};
        alignas(std::hardware_destructive_interference_size) std::atomic<std::uint64_t> data_tail{0};
};

struct ContainerDbObject {
        ContainerConfig config{};
        std::string name{};
        std::string image{};
        std::string status{};
        std::string created_at{};
};

struct DbResult {
        std::string key{};
        std::string value{};
};

struct DbStatus {
        public:
                auto ok() const -> bool {
                        return m_ok;
                }
                auto get_error() const -> std::string {
                        return m_error;
                }
                auto get_result() const -> const std::vector<DbResult>& {
                        return m_results;
                }
                auto wait() const -> void {
                        processed.wait(false, std::memory_order_acquire);
                }
        private:
                friend class DatabaseJobProcessor;
                std::vector<DbResult> m_results{};
                std::string m_error{};
                bool m_ok{false};
                std::atomic<bool> processed{false};
};

struct SubIDRange {
    uint32_t start{};
    uint32_t count{};
};

struct LayerCache {
        std::string hash{};
        std::string diff_id{};
        std::string lower_dir{};
        int64_t blob_size{0};
};

struct ImageMetadata {
        std::string id{};
        std::string name{};
        std::string tag{};
        std::string digest{};
        std::string path{};
        uint64_t size_bytes{};
        int64_t created_at{};
        std::string architecture{"amd64"};
        std::string source{};
};
