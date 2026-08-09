#pragma once
#include <span>
#include <string>

namespace CommandLineHandler {
        auto run(std::span<std::string>) -> void;
        auto ps(std::span<std::string>) -> void;
        auto remove(std::span<std::string>) -> void;
        auto inspect(std::span<std::string>) -> void;
        auto pause(std::span<std::string>) -> void;
        auto unpause(std::span<std::string>) -> void;
        auto attach(std::span<std::string>) -> void;
        auto ports(std::span<std::string>) -> void;
        auto start(std::span<std::string>) -> void;
        auto stop(std::span<std::string>) -> void;
        auto prune(std::span<std::string>) -> void;
}
