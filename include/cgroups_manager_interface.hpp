#pragma once
#include <unistd.h>
#include <cstdint>
#include <string>

class CGroupsManagerInterface {
        public:
                struct IOLimits {
                        std::uint64_t rbps{0};
                        std::uint64_t wbps{0};
                        std::uint64_t riops{0};
                        std::uint64_t wiops{0};
                };
                CGroupsManagerInterface() = default;
                virtual ~CGroupsManagerInterface() = default;
                CGroupsManagerInterface(const CGroupsManagerInterface&) = delete;
                CGroupsManagerInterface(CGroupsManagerInterface&&) = delete;
                auto operator=(const CGroupsManagerInterface&) -> CGroupsManagerInterface& = delete;
                auto operator=(CGroupsManagerInterface&&) -> CGroupsManagerInterface& = delete;

                virtual auto attach_process(pid_t) -> void = 0;
                virtual auto set_cpu_limit(int, std::uint64_t) -> void = 0;
                virtual auto set_cpu_weight(std::uint64_t) -> void = 0;
                virtual auto set_memory_max(std::uint64_t) -> void = 0;
                virtual auto set_memory_swap(std::uint64_t) -> void = 0;
                virtual auto set_io_max(std::uint64_t, std::uint64_t, const IOLimits&) -> void = 0;
                virtual auto set_io_weight(std::uint64_t, std::uint64_t, std::uint64_t) -> void = 0;
                virtual auto set_pid_limit(std::uint64_t) -> void = 0;
                virtual auto set_cpuset_cpus(const std::string&) -> void = 0;
                virtual auto set_cpuset_mems(const std::string&) -> void = 0;
                virtual auto set_freeze(const std::string&) -> void = 0;
};
