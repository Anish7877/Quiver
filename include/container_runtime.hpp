#pragma once
#include <filesystem>
#include "types.hpp"
namespace fs = std::filesystem;

class ContainerRuntime {
        public:
                explicit ContainerRuntime();
                ~ContainerRuntime() = default;
                ContainerRuntime(const ContainerRuntime&) = delete;
                ContainerRuntime(ContainerRuntime&&) = delete;
                auto operator=(const ContainerRuntime&&) -> ContainerRuntime& = delete;
                auto operator=(ContainerRuntime&&) -> ContainerRuntime& = delete;

                auto exec_commands() -> void;
                auto run_container() -> void;
                auto pause_container() -> void;
                auto unpause_container() -> void;
                auto restart_container() -> void;
        private:
                ContainerConfig m_container_config{};
                VolumeType m_volumes_list{};
                DeviceType m_devices_list{};
                NetworkType m_ports_list{};
                fs::path m_filesystem_path{};
};
