#pragma once
#include <filesystem>
namespace fs = std::filesystem;

class CGroupsManager {
        public:
                struct IOLimits {
                        uint64_t rbps{0};
                        uint64_t wbps{0};
                        uint64_t riops{0};
                        uint64_t wiops{0};
                };
                explicit CGroupsManager(const std::string&);
                ~CGroupsManager();
                CGroupsManager(const CGroupsManager&) = delete;
                CGroupsManager(CGroupsManager&&) = delete;
                auto operator=(const CGroupsManager&) -> CGroupsManager& = delete;
                auto operator=(CGroupsManager&&) -> CGroupsManager& = delete;

                auto attach_process(pid_t) -> void;
                auto set_cpu_limit(int, uint64_t) -> void;
                auto set_cpu_weight(uint64_t) -> void;
                auto set_memory_max(uint64_t) -> void;
                auto set_memory_swap(uint64_t) -> void;
                auto set_io_max(uint64_t, uint64_t, const IOLimits&) -> void;
                auto set_io_weight(uint64_t, uint64_t, uint64_t) -> void;
                auto set_pid_limit(uint64_t) -> void;
                auto set_cpuset_cpus(const std::string&) -> void;
                auto set_cpuset_mems(const std::string&) -> void;
                auto set_freeze(const std::string&) -> void;
                auto destroy() -> void;
        private:
                auto write_cgroups_file(const std::string&, const std::string&) -> void;
                auto resolve_cgroups_path() -> fs::path;
                fs::path m_cgroups_path{};
                std::string m_container_id{};
                bool m_should_destroy{false};
};
