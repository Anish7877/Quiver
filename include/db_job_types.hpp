#pragma once
#include <cstdint>
#include <atomic>
constexpr std::size_t QUEUE_SIZE{25000};

template<typename T>
struct RingBuffer{
        alignas(64) std::atomic<std::size_t> head{};
        alignas(64) std::atomic<std::size_t> tail{};
        T slots[QUEUE_SIZE];
};

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

struct ValueHeapHeader {
        std::atomic<std::uint64_t> data_tail{0};
        std::atomic<std::uint64_t> data_head{0};
};

