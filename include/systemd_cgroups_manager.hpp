#pragma once
#include "cgroups_manager_interface.hpp"
#include <sdbus-c++/Types.h>

class SystemdCGroupsManager : public CGroupsManagerInterface {
        public:
                explicit SystemdCGroupsManager(const std::string& pid) : m_container_id{pid} {}
                ~SystemdCGroupsManager() = default;
                SystemdCGroupsManager(const SystemdCGroupsManager&) = delete;
                SystemdCGroupsManager(SystemdCGroupsManager&&) = delete;
                auto operator=(const SystemdCGroupsManager&) -> SystemdCGroupsManager& = delete;
                auto operator=(SystemdCGroupsManager&&) -> SystemdCGroupsManager& = delete;

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
                auto stop() -> void override;
        private:
                auto update_dbus_property(const std::string&, const sdbus::Variant&) -> void;
                std::string m_container_id{};
};
