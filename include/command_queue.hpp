#pragma once
#include <string>
#include <optional>

template<typename T>
class CommandQueue {
        public:
                CommandQueue() = default;
                virtual ~CommandQueue() = default;
                CommandQueue(const CommandQueue&) = delete;
                CommandQueue(CommandQueue&&) = delete;
                auto operator=(const CommandQueue&) -> CommandQueue& = delete;
                auto operator=(CommandQueue&&) -> CommandQueue& = delete;

                virtual auto map_buffer(const std::string&, bool) -> void = 0;
                virtual auto atomic_push(const T&) -> bool = 0;
                virtual auto atomic_pop() -> std::optional<T> = 0;
                virtual auto get_active_connections() const -> std::size_t = 0;
                virtual auto is_empty() const -> bool = 0;
};
