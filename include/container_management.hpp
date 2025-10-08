// #pragma once

// #include "process.hpp"
// #include <chrono>
// #include <sqlite3.h>
// #include <openssl/sha.h>
// #include <sstream>
// #include <iomanip>

// class ContainerArgs {
//     public:
//         explicit ContainerArgs();
//         explicit ContainerArgs(const std::string& host_name = "container",const std::string& container_name = "container 1");

//         void set_running_status(bool running){ m_running = running; }
//         void set_attached_status(bool attached){ m_attached = attached; }
//         std::string get_container_id() const { return m_container_id; }
//         std::string get_hostname() const { return m_hostname; }
//         std::string get_container_name() const { return m_container_name; }
//         std::string get_network_name() const { return m_container_id; }
//         std::string get_filesystem_path() const { return m_filesystem_path; }
//         std::string get_volume_attached() const { return m_volume_attached; }
//         std::string get_pty_shell() const { return m_pty_shell; }
//         pid_t get_host_pid() const { return m_host_pid; }

//         ~ContainerArgs();
//     private:

//         std::string m_container_id{};
//         std::string m_hostname{};
//         std::string m_container_name{};
//         std::string m_network_name{};
//         std::string m_filesystem_path{};
//         std::string m_volume_attached{};
//         std::string m_pty_shell{};
//         pid_t m_host_pid{};

//         static long long m_ids;
//         bool m_running{};
//         bool m_attached{};
// };
// class Containers {
//     public:
//         explicit Containers();

//         void add_container() const;
//         void remove_container() const;
//         void log_container_data() const;

//         ~Containers();
//     private:
//         std::string container_id_generator(const std::string& seed) const;
//         std::string seed_generator() const;
//         void ensure_dirs() const;
//         void ensure_files() const;

//         const static std::string m_container_db_path;
//         const static std::string m_container_log_path;
// };

#pragma once

#include "database_manager.hpp"
#include <string>
#include <vector>

class ContainerManager {
public:
    explicit ContainerManager(DatabaseManager& db);

    // Creates a container entry in the DB with initial config
    std::string create_container(const std::string& image_name, const std::string& container_name = "", const std::string& hostname = "container");

    // Retrieves full container information
    Container get_container_info(const std::string& container_id);

    // Lists all containers
    std::vector<Container> list_all_containers();

    // Removes a container and its associated data (like volumes)
    bool remove_container(const std::string& container_id);
    
    // Logs container data to the console (restoring this functionality)
    void log_container_data(const std::string& container_id);

private:
    DatabaseManager& m_db;

    std::string generate_container_id(const std::string& seed);
};