#pragma once
#include <span>
#include <string>

namespace CommandLineHandler {
        auto run(std::span<std::string>) -> void;
        auto ps() -> void;
        auto remove(const std::string&) -> void;
        auto inspect(const std::string&) -> void;
}
