#include "database_command_queue.hpp"
#include "types.hpp"
#include <atomic>
#include <cstring>
#include <emmintrin.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

auto DatabaseCommandQueue::map_buffer(const std::string& buf_name, bool is_consumer) -> void {
        int fd{-1};
        m_is_consumer = is_consumer;
        m_buf_name = buf_name;

        long page_size{sysconf(_SC_PAGESIZE)};
        if (page_size == -1) [[unlikely]] {
                m_ok = false;
                m_error = "Database Command Queue Error: Failed to get system page size.";
                return;
        }
        std::size_t total_file_size{m_buf_size + page_size};

        if (m_is_consumer) {
                shm_unlink(m_buf_name.c_str());
                fd = shm_open(m_buf_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0660);
                if (fd == -1) [[unlikely]] {
                        m_ok = false;
                        m_error = "Database Command Queue Error: failed to create shared memory.";
                        return;
                }

                if (ftruncate(fd, total_file_size) == -1) [[unlikely]] {
                        close(fd);
                        m_ok = false;
                        m_error = "Database Command Queue Error: failed to set shared memory size.";
                        return;
                }
        }
        else {
                fd = shm_open(m_buf_name.c_str(), O_RDWR, 0660);
                if (fd == -1) [[unlikely]] {
                        m_ok = false;
                        m_error = "Database Command Queue Error: Worker failed to connect.";
                        return;
                }
        }

        void* header_addr{mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)};
        if (header_addr == MAP_FAILED) [[unlikely]] {
                close(fd);
                m_ok = false;
                m_error = "Database Command Queue Error: failed to map header memory.";
                return;
        }

        void* virtual_addr{mmap(nullptr, m_buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_size)};
        if (virtual_addr == MAP_FAILED) [[unlikely]] {
                close(fd);
                m_ok = false;
                m_error = "Database Command Queue Error: failed to reserve virtual memory.";
                return;
        }
        close(fd);

        if (m_is_consumer) {
                m_header = new (header_addr) CommandQueueHeader{};
                m_mapped_address = static_cast<JobSlot*>(virtual_addr);
                m_header->head.store(0, std::memory_order_relaxed);
                m_header->tail.store(0, std::memory_order_relaxed);
                m_header->connections.store(0, std::memory_order_relaxed);
                for (std::size_t i{0}; i<QUEUE_SIZE; ++i){
                        new (&m_mapped_address[i]) JobSlot{};
                        m_mapped_address[i].state.store(SlotState::EMPTY, std::memory_order_relaxed);
                }
        }
        else {
                m_header = reinterpret_cast<CommandQueueHeader*>(header_addr);
                m_mapped_address = static_cast<JobSlot*>(virtual_addr);
                m_header->connections.fetch_add(1, std::memory_order_relaxed);
        }
}

auto DatabaseCommandQueue::atomic_push(const DatabaseJobData& data) -> bool {
        std::size_t current_tail{m_header->tail.load(std::memory_order_relaxed)};

        while (true) {
                std::size_t current_head{m_header->head.load(std::memory_order_acquire)};
                if (current_tail - current_head >= QUEUE_SIZE) {
                        return false;
                }
                if (m_header->tail.compare_exchange_weak(current_tail, current_tail + 1, std::memory_order_relaxed)) {
                        break;
                }
        }

        std::size_t index{current_tail % QUEUE_SIZE};
        JobSlot& slot{m_mapped_address[index]};

        slot.type = data.type;
        std::memcpy(slot.key, data.key, sizeof(slot.key));
        std::memcpy(slot.path, data.path, sizeof(slot.path));
        slot.value_offset = data.value_offset;
        slot.value_length = data.value_length;
        slot.target = data.target;
        slot.state.store(SlotState::READY, std::memory_order_release);

        return true;
}

auto DatabaseCommandQueue::atomic_pop() -> std::optional<DatabaseJobData> {
        std::size_t current_head{m_header->head.load(std::memory_order_relaxed)};
        std::size_t current_tail{m_header->tail.load(std::memory_order_acquire)};

        if (current_head == current_tail) {
                return std::nullopt;
        }

        std::size_t index{current_head % QUEUE_SIZE};
        JobSlot& acquired_slot{m_mapped_address[index]};

        while (acquired_slot.state.load(std::memory_order_acquire) != SlotState::READY) {
                _mm_pause();
        }

        DatabaseJobData local_copy{};
        std::memcpy(local_copy.key, acquired_slot.key, sizeof(local_copy.key));
        std::memcpy(local_copy.path, acquired_slot.path, sizeof(local_copy.path));
        local_copy.type = acquired_slot.type;
        local_copy.target = acquired_slot.target;
        local_copy.value_offset = acquired_slot.value_offset;
        local_copy.value_length = acquired_slot.value_length;

        acquired_slot.state.store(SlotState::EMPTY, std::memory_order_release);

        m_header->head.store(current_head + 1, std::memory_order_release);

        return local_copy;
}

auto DatabaseCommandQueue::get_active_connections() const -> std::size_t {
        if (!m_header) return 0;
        return m_header->connections.load(std::memory_order_acquire);
}

auto DatabaseCommandQueue::is_empty() const -> bool {
        std::uint64_t current_head{m_header->head.load(std::memory_order_acquire)};
        std::uint64_t current_tail{m_header->tail.load(std::memory_order_acquire)};
        return current_head == current_tail;
}

DatabaseCommandQueue::~DatabaseCommandQueue() {
        std::uint64_t active_connections{0};
        if(!m_is_consumer && m_header != nullptr) {
                m_header->connections.fetch_sub(1, std::memory_order_release);
        }

        if (m_header != nullptr) {
                active_connections = m_header->connections.load(std::memory_order_acquire);
                long page_size{sysconf(_SC_PAGESIZE)};
                if (munmap(m_header, page_size) == -1) {
                        std::cerr << "Database Command Queue Error: failed to unmap header.\n";
                }
                m_header = nullptr;
        }

        if(m_mapped_address != nullptr) {
                if(munmap(m_mapped_address, m_buf_size) == -1) {
                        std::cerr << "Database Command Queue Error : munmap failed to unmap the mapped addresses.\n";
                }
                m_mapped_address = nullptr;
        }

        if(active_connections == 0 && m_is_consumer && !m_buf_name.empty()) {
                if(shm_unlink(m_buf_name.c_str()) == -1) {
                        std::cerr << "Database Command Queue Error : shared memory unlink failed.\n";
                }
        }
}
