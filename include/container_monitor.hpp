#pragma once
#include "singleton.hpp"
#include "logger.hpp"

class ContainerMonitor : public Singleton<ContainerMonitor> {
        friend class Singleton<ContainerMonitor>;
        private:
                ContainerMonitor() = default;
                ~ContainerMonitor();
        public:
                ContainerMonitor(const ContainerMonitor&) = delete;
                ContainerMonitor(ContainerMonitor&&) = delete;
                auto operator=(const ContainerMonitor&) -> ContainerMonitor& = delete;
                auto operator=(ContainerMonitor&&) -> ContainerMonitor& = delete;

                auto setup_usernamespace() -> void;
        private:
                auto setup_uid_map() -> void;
                auto setup_gid_map() -> void;
                auto attach_to_container() -> void;
                Logger m_logger{};
};
