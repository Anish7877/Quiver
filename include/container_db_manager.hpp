#pragma once
#include "singleton.hpp"
#include "types.hpp"

class DatabaseCommandQueue;
class ValueHeap;
class ContainerDbManager : public Singleton<ContainerDbManager> {
        friend class Singleton<ContainerDbManager>;
        private:
                ContainerDbManager() = default;
                ~ContainerDbManager() = default;
        public:
                ContainerDbManager(const ContainerDbManager&) = delete;
                ContainerDbManager(ContainerDbManager&&) = delete;
                auto operator=(const ContainerDbManager&) -> ContainerDbManager& = delete;
                auto operator=(ContainerDbManager&&) -> ContainerDbManager& = delete;

                auto add_container(const ContainerDbObject&) -> bool;
                auto update_container() -> bool;
                auto list_all_container() -> bool;
                auto list_all_running_container() -> bool;
                auto remove_container() -> bool;
        private:
                DatabaseCommandQueue* m_db_command_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
};
