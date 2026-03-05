#pragma once
#include "types.hpp"
#include "singleton.hpp"
#include <string>

class DatabaseValueHeap : public Singleton<DatabaseValueHeap> {
        friend class Singleton<DatabaseValueHeap>;
        private:
                DatabaseValueHeap() = default;
                ~DatabaseValueHeap();
        public:
                DatabaseValueHeap(const DatabaseValueHeap&) = delete;
                DatabaseValueHeap(DatabaseValueHeap&&) = delete;
                auto operator=(const DatabaseValueHeap&) -> DatabaseValueHeap& = delete;
                auto operator=(DatabaseValueHeap&&) -> DatabaseValueHeap& = delete;

                auto map_buffer(const std::string& buf_name, std::size_t physical_size, bool is_consumer) -> void;
                auto write_job_data(const std::string& buf, std::size_t& out_offset) -> bool;
                auto get_job_data_pointer(std::size_t offset) const -> const char*;
                auto commit_read_head(std::size_t bytes_processed) -> void;
        private:
                char* m_mapped_mirror_address{nullptr};
                std::size_t m_physical_size{};
                std::string m_buf_name{};
                bool m_is_consumer{};
                ValueHeapHeader* m_header{nullptr};
};
