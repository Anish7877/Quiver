#pragma once
#include "types.hpp"
#include "singleton.hpp"
#include <string>

class ValueHeap : public Singleton<ValueHeap> {
        friend class Singleton<ValueHeap>;
        private:
                ValueHeap() = default;
                ~ValueHeap();
        public:
                static constexpr std::size_t VALUE_HEAP_SIZE{100 * 1024 * 1024};
                ValueHeap(const ValueHeap&) = delete;
                ValueHeap(ValueHeap&&) = delete;
                auto operator=(const ValueHeap&) -> ValueHeap& = delete;
                auto operator=(ValueHeap&&) -> ValueHeap& = delete;

                auto map_buffer(const std::string&, std::size_t, bool) -> void;
                auto write_job_data(const std::string&, std::size_t&) -> bool;
                auto commit_read_head(std::size_t) -> void;
                auto get_job_data_pointer(std::size_t) const -> const char*;
                auto ok() const -> bool { return m_ok; }
                auto get_error() const -> std::string { return m_error; }
        private:
                std::string m_buf_name{};
                std::string m_error{};
                std::size_t m_physical_size{};
                char* m_mapped_mirror_address{nullptr};
                ValueHeapHeader* m_header{nullptr};
                bool m_ok{true};
                bool m_is_consumer{};
};
