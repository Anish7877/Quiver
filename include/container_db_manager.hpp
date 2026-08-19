#pragma once
#include "singleton.hpp"
#include "types.hpp"
#include <optional>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

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

                auto init() -> void;
                auto add_container(const ContainerDbObject&) -> void;
                auto remove_container(const std::string&) -> void;
                auto update_container(const std::string&, const ContainerDbObject&) -> void;
                auto list_all_container() -> void;
                auto list_all_running_container() -> void;
                [[nodiscard]] auto get_container(const std::string&) -> std::optional<ContainerDbObject>;
                [[nodiscard]] auto get_all_container() -> std::vector<ContainerDbObject>;
                auto inspect_container(const std::string&) -> void;
        private:
                auto extract_metadata(const std::string&) -> std::optional<ContainerDbObject>;
                DatabaseCommandQueue* m_db_command_queue{nullptr};
                ValueHeap* m_value_heap{nullptr};
};
