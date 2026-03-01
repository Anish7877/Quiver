#include "database_value_heap.hpp"
#include "db_job_types.hpp"
#include "utils.hpp"
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

auto DatabaseValueHeap::map_buffer(const std::string& buf_name, std::size_t physical_size, bool is_consumer) -> void {
        int fd{-1};
        m_is_consumer = is_consumer;
        m_physical_size = physical_size;
        m_buf_name = buf_name;

        if(m_is_consumer) {
                shm_unlink(m_buf_name.c_str());
                fd = shm_open(m_buf_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0660);
                if(fd == CERR) [[unlikely]] {
                        throw std::runtime_error("Value Heap Error: failed to create shared memory.");
                }
                if(ftruncate(fd, m_physical_size) == CERR) [[unlikely]] {
                        close(fd);
                        throw std::runtime_error("Value Heap Error: failed to set shared memory size.");
                }
        }
        else {
                fd = shm_open(m_buf_name.c_str(), O_RDWR, 0660);
                if(fd == CERR) [[unlikely]] {
                        throw std::runtime_error("Value Heap Error: Worker failed to connect.");
                }
        }

        void* virtual_addr{mmap(nullptr, 2 * m_physical_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)};
        if (virtual_addr == MAP_FAILED) [[unlikely]] {
                close(fd);
        }

        void* first_half{mmap(virtual_addr, m_physical_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0)};
        void* second_half{mmap(static_cast<char*>(virtual_addr) + m_physical_size, m_physical_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0)};

        close(fd);

        if (first_half == MAP_FAILED || second_half == MAP_FAILED) [[unlikely]] {
                throw std::runtime_error("Value Heap Error: failed to mirror memory mapping.");
        }
        m_mapped_mirror_address = static_cast<char*>(virtual_addr);

        m_header = reinterpret_cast<ValueHeapHeader*>(m_mapped_mirror_address);
        if(m_is_consumer) {
                std::size_t start_offset{sizeof(ValueHeapHeader)};
                m_header->data_head.store(start_offset, std::memory_order_relaxed);
                m_header->data_tail.store(start_offset, std::memory_order_relaxed);
        }
}

auto DatabaseValueHeap::write_job_data(const std::string& buf, std::size_t& out_offset) -> bool {
        std::size_t current_tail{m_header->data_tail.load(std::memory_order_relaxed)};

        while (true) {
                std::size_t current_head{m_header->data_head.load(std::memory_order_acquire)};

                if ((current_tail - current_head) + buf.size() > m_physical_size) {
                        return false;
                }

                if (m_header->data_tail.compare_exchange_weak(current_tail, current_tail + buf.size(), std::memory_order_relaxed)) {
                        break;
                }
        }
        out_offset = m_header->data_tail.fetch_add(buf.size(), std::memory_order_relaxed);

        std::size_t physical_start{out_offset % m_physical_size};
        char* destination_addr{m_mapped_mirror_address + physical_start};

        std::memcpy(destination_addr, buf.c_str(), buf.size());
        return true;
}

auto DatabaseValueHeap::get_job_data_pointer(std::size_t offset) const -> const char* {
        std::size_t physical_start{offset % m_physical_size};
        return m_mapped_mirror_address + physical_start;
}

auto DatabaseValueHeap::commit_read_head(std::size_t bytes_processed) -> void {
        std::size_t current_head{m_header->data_head.load(std::memory_order_relaxed)};
        std::size_t next_head{current_head + bytes_processed};
        m_header->data_head.store(next_head, std::memory_order_release);
}
