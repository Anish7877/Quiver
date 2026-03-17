#include "logger_command_queue.hpp"
#include "types.hpp"
#include <atomic>
#include <cstring>
#include <iostream>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

auto LoggerCommandQueue::map_buffer(const std::string& buf_name, bool is_consumer) -> void {
        int fd{-1};
        m_is_consumer = is_consumer;
        m_bufname = buf_name;

        long page_size{sysconf(_SC_PAGESIZE)};
        if (page_size == -1) [[unlikely]] {
                m_ok = false;
		m_error = "Logger Command Queue Error: Failed to get system page size.";
		return;
        }
        std::size_t total_file_size{m_buf_size + page_size};

        if (m_is_consumer) {
                shm_unlink(m_bufname.c_str());
                fd = shm_open(m_bufname.c_str(), O_CREAT | O_EXCL | O_RDWR, 0660);
                if (fd == -1) [[unlikely]] {
                        m_ok = false;
                        m_error = "Logger Command Queue Error: failed to create shared memory.";
                        return;
                }

                if (ftruncate(fd, total_file_size) == -1) [[unlikely]] {
                        close(fd);
                        m_ok = false;
                        m_error = "Logger Command Queue Error: failed to set shared memory size.";
                        return;
                }
        }
        else {
                fd = shm_open(m_bufname.c_str(), O_RDWR, 0660);
                if (fd == -1) [[unlikely]] {
                        m_ok = false;
                        m_error = "Logger Command Queue Error: Worker failed to connect.";
                        return;
                }
        }

        void* header_addr{mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)};
        if (header_addr == MAP_FAILED) [[unlikely]] {
                close(fd);
                m_ok = false;
                m_error = "Logger Command Queue Error: failed to map header memory.";
                return;
        }

        void* virtual_addr{mmap(nullptr, m_buf_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_size)};
        if (virtual_addr == MAP_FAILED) [[unlikely]] {
                close(fd);
                m_ok = false;
                m_error = "Logger Command Queue Error: failed to reserve virtual memory.";
                return;
        }
        close(fd);

        if (m_is_consumer) {
                m_header = new (header_addr) CommandQueueHeader{};
                m_mapped_address = static_cast<LogSlot*>(virtual_addr);
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
                m_mapped_address = static_cast<LogSlot*>(virtual_addr);
                m_header->connections.fetch_add(1, std::memory_order_relaxed);
        }
}

auto LoggerCommandQueue::atomic_push(const LogJobData& data) -> bool {
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
        LogSlot& slot{m_mapped_address[index]};

        slot.target_log = data.target_log;
        slot.value_offset = data.value_offset;
        slot.value_length = data.value_length;
        slot.state.store(SlotState::READY, std::memory_order_release);

        return true;
}

auto LoggerCommandQueue::atomic_pop() -> std::optional<LogJobData> {
        std::size_t current_head{m_header->head.load(std::memory_order_relaxed)};
        std::size_t current_tail{m_header->tail.load(std::memory_order_acquire)};

        if (current_head == current_tail) {
                return std::nullopt;
        }

        std::size_t index{current_head % QUEUE_SIZE};
        LogSlot& acquired_slot{m_mapped_address[index]};

        if (acquired_slot.state.load(std::memory_order_acquire) != SlotState::READY) {
                return std::nullopt;
        }

        LogJobData local_copy{};
        local_copy.target_log = acquired_slot.target_log;
        local_copy.value_offset = acquired_slot.value_offset;
        local_copy.value_length = acquired_slot.value_length;

        acquired_slot.state.store(SlotState::EMPTY, std::memory_order_release);

        m_header->head.store(current_head + 1, std::memory_order_release);

        return local_copy;
}

auto LoggerCommandQueue::get_active_connections() const -> std::size_t {
        if (!m_header) return 0;
        return m_header->connections.load(std::memory_order_acquire);
}

auto LoggerCommandQueue::is_empty() const -> bool {
        std::uint64_t current_head{m_header->head.load(std::memory_order_acquire)};
        std::uint64_t current_tail{m_header->tail.load(std::memory_order_acquire)};
        return current_head == current_tail;
}

LoggerCommandQueue::~LoggerCommandQueue() {
        if(!m_is_consumer && m_header != nullptr) {
                m_header->connections.fetch_sub(1, std::memory_order_release);
        }

        if (m_header != nullptr) {
                long page_size{sysconf(_SC_PAGESIZE)};
                if (munmap(m_header, page_size) == -1) {
                        std::cerr << "Logger Command Queue Error: failed to unmap header.\n";
                }
                m_header = nullptr;
        }

        if(m_mapped_address != nullptr) {
                if(munmap(m_mapped_address, m_buf_size) == -1) {
                        std::cerr << "Logger Command Queue Error : munmap failed to unmap the mapped addresses.\n";
                }
                m_mapped_address = nullptr;
        }

        if(m_is_consumer && !m_bufname.empty()) {
                if(shm_unlink(m_bufname.c_str()) == -1) {
                        std::cerr << "Logger Command Queue Error : shared memory unlink failed.\n";
                }
        }
}
