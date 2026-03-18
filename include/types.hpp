#pragma once
#include <cstdint>
#include <atomic>
#include <string>
#include <new>
#include <utility>
#include <vector>

enum class JobType : std::uint8_t {
        GET = 0,
        PUT = 1,
        UPDATE = 2,
        DELETE = 3
};

enum class SlotState : std::uint32_t {
        EMPTY = 0,
        WRITING = 1,
        READY = 2
};

enum class TargetDB : std::uint8_t {
        CONTAINER = 0,
        VOLUME = 1,
        DEVICE = 2,
        NETWORK = 3,
        IMAGE = 4
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

struct Status {
        public:
                auto ok() const -> bool {
                        return m_ok;
                }
                auto get_error() const -> std::string {
                        return m_error;
                }
                auto get_result() const -> std::string {
                        return m_result;
                }
        private:
                friend class ContainerManager;
                friend class VolumeManager;
                friend class DeviceManager;
                friend class NetworkManager;
                friend class ImageManager;
                std::string m_result{};
                std::string m_error{};
                bool m_ok{false};
};

struct ContainerConfig {
        //TODO: parsed manifest information
};

struct ContainerType {
        std::uint32_t pid{};
        std::uint32_t net_pid{};
        bool vfs{};
        bool no_remove{};
        std::string id{};
        std::string name{};
        std::string image{};
        std::string status{};
        std::string created_at{};
        std::string hostname{};
        std::string filesystem_path{};
        std::string pty_shell{};
        std::string vfs_path{};
        std::vector<std::pair<std::string, std::string>> volumes{};
        std::vector<std::string> devices{};
        std::vector<std::pair<int, int>> ports{};
};

struct VolumeType {
        std::string container_id{};
        std::string created_at{};
        std::vector<std::pair<std::string, std::string>> volumes{};
};

struct DeviceType {
        std::string container_id{};
        std::string created_at{};
        std::vector<std::string> devices{};
};

struct NetworkType{
        std::string container_id{};
        std::string created_at{};
        std::vector<std::pair<int, int>> ports{};
};

struct ImageType {
        std::string id{};
        std::string name{};
        std::string tag{};
        std::string path{};
        std::string created_at{};
};

