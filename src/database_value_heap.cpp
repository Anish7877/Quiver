#include "database_value_heap.hpp"
#include "db_types.hpp"
#include "utils.hpp"
#include <iostream>
#include <atomic>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

auto DatabaseValueHeap::map_buffer(const std::string& buf_name, std::size_t physical_size, bool is_consumer) -> void {
        int fd{-1};
        m_is_consumer = is_consumer;
        m_buf_name = buf_name;

        long page_size{sysconf(_SC_PAGESIZE)};
        if (page_size == CERR) [[unlikely]] {
                throw std::runtime_error("Value Heap Error: Failed to get system page size.");
        }
        std::size_t remainder{physical_size % page_size};
        m_physical_size = (remainder == 0) ? physical_size : physical_size + (page_size - remainder);
        std::size_t total_file_size{m_physical_size + page_size};

        if (m_is_consumer) {
                shm_unlink(m_buf_name.c_str());
                fd = shm_open(m_buf_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0660);
                if (fd == CERR) [[unlikely]] {
                        throw std::runtime_error("Value Heap Error: failed to create shared memory.");
                }

                if (ftruncate(fd, total_file_size) == CERR) [[unlikely]] {
                        close(fd);
                        throw std::runtime_error("Value Heap Error: failed to set shared memory size.");
                }
        }
        else {
                fd = shm_open(m_buf_name.c_str(), O_RDWR, 0660);
                if (fd == CERR) [[unlikely]] {
                        throw std::runtime_error("Value Heap Error: Worker failed to connect.");
                }
        }

        void* header_addr{mmap(nullptr, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)};
        if (header_addr == MAP_FAILED) [[unlikely]] {
                close(fd);
                throw std::runtime_error("Value Heap Error: failed to map header memory.");
        }

        void* virtual_addr{mmap(nullptr, 2 * m_physical_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)};
        if (virtual_addr == MAP_FAILED) [[unlikely]] {
                close(fd);
                throw std::runtime_error("Value Heap Error: failed to reserve virtual memory.");
        }

        void* first_half{mmap(virtual_addr, m_physical_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, page_size)};
        void* second_half{mmap(static_cast<char*>(virtual_addr) + m_physical_size, m_physical_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, page_size)};

        close(fd);

        if (first_half == MAP_FAILED || second_half == MAP_FAILED) [[unlikely]] {
                throw std::runtime_error("Value Heap Error: failed to mirror memory mapping.");
        }

        if (m_is_consumer) {
                m_header = new (header_addr) ValueHeapHeader{};
                m_header->data_head.store(0, std::memory_order_relaxed);
                m_header->data_tail.store(0, std::memory_order_relaxed);
                m_mapped_mirror_address = static_cast<char*>(virtual_addr);
        }
        else {
                m_header = reinterpret_cast<ValueHeapHeader*>(header_addr);
                m_mapped_mirror_address = static_cast<char*>(virtual_addr);
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
        out_offset = current_tail;

        std::size_t physical_start{out_offset % m_physical_size};
        char* destination_addr{m_mapped_mirror_address + physical_start};

        std::memcpy(destination_addr, buf.data(), buf.size());
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

DatabaseValueHeap::~DatabaseValueHeap() {
        if (m_header != nullptr) {
                long page_size{sysconf(_SC_PAGESIZE)};
                if (munmap(m_header, page_size) == CERR) {
                        std::cerr << "Value Heap Warning: Failed to munmap header.\n";
                }
                m_header = nullptr;
        }

        if (m_mapped_mirror_address != nullptr) {
                if (munmap(m_mapped_mirror_address, 2 * m_physical_size) == CERR) {
                        std::cerr << "Value Heap Warning: Failed to munmap mirror.\n";
                }
                m_mapped_mirror_address = nullptr;
        }

        if (m_is_consumer && !m_buf_name.empty()) {
                if (shm_unlink(m_buf_name.c_str()) == CERR) {
                        std::cerr << "Value Heap Warning: Failed to shm_unlink file.\n";
                }
        }
}
