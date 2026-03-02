#pragma once
#include <cstdint>
#include <atomic>

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

struct alignas(64) JobSlot {
        std::atomic<SlotState> state{};
        char key[32]{};
        JobType type{};
        std::uint64_t value_offset{};
        std::uint32_t value_length{};
};

struct JobData {
        char key[32]{};
        JobType type{};
        std::uint64_t value_offset{};
        std::uint32_t value_length{};
};

struct CommandQueueHeader {
        std::atomic<std::uint64_t> tail{0};
        std::atomic<std::uint64_t> head{0};
        std::atomic<std::uint64_t> connections{0};
};

struct ValueHeapHeader {
        std::atomic<std::uint64_t> data_tail{0};
        std::atomic<std::uint64_t> data_head{0};
};

