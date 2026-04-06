#pragma once
#include "cgroups_manager_interface.hpp"
#include <filesystem>
namespace fs = std::filesystem;

class RawCGroupsManager : public CGroupsManagerInterface {
        public:
                explicit RawCGroupsManager(const std::string&, const fs::path&);
                ~RawCGroupsManager();
                RawCGroupsManager(const RawCGroupsManager&) = delete;
                RawCGroupsManager(RawCGroupsManager&&) = delete;
                auto operator=(const RawCGroupsManager&) -> RawCGroupsManager& = delete;
                auto operator=(RawCGroupsManager&&) -> RawCGroupsManager& = delete;

                auto attach_process(pid_t) -> void override;
                auto set_cpu_limit(int, std::uint64_t) -> void override;
                auto set_cpu_weight(std::uint64_t) -> void override;
                auto set_memory_max(std::uint64_t) -> void override;
                auto set_memory_swap(std::uint64_t) -> void override;
                auto set_io_max(std::uint64_t, std::uint64_t, const IOLimits&) -> void override;
                auto set_io_weight(std::uint64_t, std::uint64_t, std::uint64_t) -> void override;
                auto set_pid_limit(std::uint64_t) -> void override;
                auto set_cpuset_cpus(const std::string&) -> void override;
                auto set_cpuset_mems(const std::string&) -> void override;
                auto set_freeze(const std::string&) -> void override;
        private:
                auto write_cgroups_file(const std::string&, const std::string&) -> void;
                auto resolve_cgroups_path(const fs::path& delegated_path) -> fs::path;
                fs::path m_cgroups_path{};
                std::string m_container_id{};
};
