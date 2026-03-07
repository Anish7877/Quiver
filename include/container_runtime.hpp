#pragma once
#include "types.hpp"

class ContainerRuntime {
        public:
                explicit ContainerRuntime();
                ~ContainerRuntime() = default;
                ContainerRuntime(const ContainerRuntime&) = delete;
                ContainerRuntime(ContainerRuntime&&) = delete;
                auto operator=(const ContainerRuntime&&) -> ContainerRuntime& = delete;
                auto operator=(ContainerRuntime&&) -> ContainerRuntime& = delete;

                auto exec_commands() -> void;
                auto pause_container() -> void;
                auto unpause_container() -> void;
                auto restart_container() -> void;
                auto run_container() -> void;
        private:
                ContainerConfig m_container_config{};
};
