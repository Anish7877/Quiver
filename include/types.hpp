   #pragma once
#include "container_config.hpp"
#include <cstdint>
#include <atomic>
#include <string>
#include <new>
#include "cgroups_manager_interface.hpp"

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
        char path[64]{};
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
        char key[32]{};
        char path[64]{};
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

struct IOMaxUpdate {
        std::uint64_t major{};
        std::uint64_t minor{};
        CGroupsManagerInterface::IOLimits limits{};
};

struct IOWeightUpdate {
        std::uint64_t major{};
        std::uint64_t minor{};
        std::uint64_t weight{};
};

struct ContainerDbObject {
        ContainerConfig config{};
        std::string name{};
        std::string image{};
        std::string status{};
        std::string created_at{};
        pid_t pid{};
        long boot_time{};
        int cpu_quota{-1};
        std::uint64_t cpu_period{100000};
        std::uint64_t cpu_weight{};
        std::uint64_t memory_max{};
        std::uint64_t memory_swap{};
        std::uint64_t pids_limit{};
        std::string cpuset_cpus{};
        std::string cpuset_mems{};
        std::vector<IOMaxUpdate> io_max_updates{};
        std::vector<IOWeightUpdate> io_weight_updates{};
};

struct DbResult {
        std::string key{};
        std::string value{};
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
