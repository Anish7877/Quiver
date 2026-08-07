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
}
