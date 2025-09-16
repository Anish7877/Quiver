#pragma once

#include <string>
#include <vector>
#include <unistd.h>
#include <sqlite3.h>

// Data structure to hold information about a single container.
struct Container {
    std::string id;
    std::string name;
    std::string image;
    std::string status;
    pid_t pid;
    std::string created_at;
};

class ContainerManager {
public:
    ContainerManager();
    ~ContainerManager();

    std::string create_container(const std::string& image_name, std::string& error);
    bool start_container(const std::string& id, pid_t pid, std::string& error);
    std::vector<Container> list_containers(std::string& error);
    // Add this new function to look up a container's PID.
    pid_t get_container_pid(const std::string& id_or_name, std::string& error);

private:
    bool init_database(std::string& error);
    std::string generate_container_id();

    std::string m_db_path;
    sqlite3* m_db; // A pointer to the active SQLite database connection.
};

