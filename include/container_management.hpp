#pragma once

#include "database_manager.hpp"
#include <string>
#include <vector>

class ContainerManager {
    public:
        explicit ContainerManager(DatabaseManager& db);

        std::string create_container(const std::string& container_id, const pid_t& container_pid, const std::string& container_name, const std::string& filesystem_path, const std::string& image_name);

        ContainerObject get_container_info(const std::string& container_id);

        std::vector<ContainerObject> list_all_containers();

        bool remove_container(const std::string& container_id);

        void log_container_data(const std::string& container_id);

    private:
        DatabaseManager& m_db;
        std::string generate_container_id(const std::string& seed);
};
