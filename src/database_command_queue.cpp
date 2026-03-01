#include "database_command_queue.hpp"
#include "db_job_types.hpp"
#include "utils.hpp"
#include <atomic>
#include <cstring>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

auto DatabaseCommandQueue::map_buffer(const std::string& buf_name, bool is_consumer) -> void {
        int fd{-1};
        m_buf_name = buf_name;
        m_is_consumer = is_consumer;
        if(is_consumer) {
                shm_unlink(m_buf_name.c_str());
                fd = shm_open(m_buf_name.c_str(), O_CREAT | O_RDWR, 0660);
                if(fd == CERR) [[unlikely]] {
                        throw std::runtime_error("Database Command Queue Error: failed to create shared memory.");
                }
                if(ftruncate(fd, m_buf_size) == CERR) [[unlikely]] {
                        close(fd);
                        throw std::runtime_error("Database Command Queue Error: failed to set shared memory size.");
                }
        }
        else {
                fd = shm_open(m_buf_name.c_str(), O_RDWR, 0660);
                if(fd == CERR) [[unlikely]] {
                        throw std::runtime_error("Database Command Queue Error: failed to connect to shared memory.");
                }
        }

        void* mapped_address{mmap(nullptr, m_buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)};
        close(fd);

        if(mapped_address == MAP_FAILED) [[unlikely]] {
                mapped_address = nullptr;
                throw std::runtime_error("Database Command Queue Error: mmap failed to map shared memory.");
        }

        m_mapped_address = mapped_address;

        if(is_consumer) {
                auto* memory{static_cast<RingBuffer<JobSlot>*>(m_mapped_address)};
                memory->head.store(0, std::memory_order_relaxed);
                memory->tail.store(0, std::memory_order_relaxed);

                for(std::size_t i{0}; i<QUEUE_SIZE; ++i) {
                        memory->slots[i].state.store(SlotState::EMPTY, std::memory_order_relaxed);
                }
        }
}

auto DatabaseCommandQueue::atomic_push(const JobData& job) -> bool {
        auto* memory{static_cast<RingBuffer<JobSlot>*>(m_mapped_address)};

        std::size_t current_tail{memory->tail.load(std::memory_order_relaxed)};
        std::size_t next_tail{};

        do {
                next_tail = (current_tail + 1) % QUEUE_SIZE;

                if(next_tail == memory->head.load(std::memory_order_relaxed)) {
                        return false;
                }

        } while(memory->tail.compare_exchange_weak(current_tail, next_tail, std::memory_order_release, std::memory_order_relaxed));

        JobSlot& aquired_slot{memory->slots[current_tail]};

        SlotState expected{SlotState::EMPTY};
        if(!aquired_slot.state.compare_exchange_strong(expected, SlotState::WRITING, std::memory_order_acquire)) {
                return false;
        }

        aquired_slot.type = job.type;
        std::memcpy(aquired_slot.key, job.key, 32);
        aquired_slot.value_length = job.value_length;
        aquired_slot.value_offset = job.value_offset;
        aquired_slot.state.store(SlotState::READY, std::memory_order_relaxed);
        return true;
}

auto DatabaseCommandQueue::atomic_pop() -> std::optional<JobData> {
        auto* memory{static_cast<RingBuffer<JobSlot>*>(m_mapped_address)};

        std::size_t current_head{memory->head.load(std::memory_order_relaxed)};
        if (current_head == memory->tail.load(std::memory_order_acquire)) {
                return std::nullopt;
        }

        JobSlot& acquired_slot{memory->slots[current_head]};
        if (acquired_slot.state.load(std::memory_order_acquire) != SlotState::READY) {
                return std::nullopt;
        }

        JobData local_copy{};
        local_copy.type = acquired_slot.type;
        std::memcpy(local_copy.key, acquired_slot.key, 32);
        local_copy.value_offset = acquired_slot.value_offset;
        local_copy.value_length = acquired_slot.value_length;
        acquired_slot.state.store(SlotState::EMPTY, std::memory_order_release);

        std::size_t next_head = (current_head + 1) % QUEUE_SIZE;
        memory->head.store(next_head, std::memory_order_relaxed);

        return local_copy;
}
