#pragma once
#include <iostream>
#include <format>
#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

template<typename Func, typename T, typename... Args>
requires std::invocable<Func, Args...> &&
         (!std::is_void_v<std::invoke_result_t<Func, Args...>>)
auto test(Func&& F, const T& expected, Args&&... args) -> void {
    try {
        auto actual{std::invoke(std::forward<Func>(F), std::forward<Args>(args)...)};
        if (actual == expected) {
            std::cout << "Test Passed!\n";
        } else {
            std::cout << std::format("Test Failed: Expected -> {} got -> {}\n", expected, actual);
        }
    }
    catch(const std::exception& e) {
        std::cout << std::format("Test Failed: {}\n", e.what());
    }
    catch(...) {
        std::cout << "Test Failed with unknown exception\n";
    }
}

template<typename Func, typename... Args>
requires std::invocable<Func, Args...> &&
         std::is_void_v<std::invoke_result_t<Func, Args...>>
auto test(Func&& F, Args&&... args) -> void {
    try {
        std::invoke(std::forward<Func>(F), std::forward<Args>(args)...);
        std::cout << "Test Passed!\n";
    }
    catch(const std::exception& e) {
        std::cout << std::format("Test Failed: {}\n", e.what());
    }
    catch(...) {
        std::cout << "Test Failed with unknown exception\n";
    }
}

namespace Tests {
        auto test_utils() -> void;
        auto test_monitor() -> void;
}
