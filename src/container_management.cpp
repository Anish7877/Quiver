#include "../include/container_management.hpp"
#include "../include/utils.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <iostream>

// Constructor that takes a reference to the DatabaseManager
ContainerManager::ContainerManager(DatabaseManager& db) : m_db(db) {}

// Creates a new container and saves it to the database
std::string ContainerManager::create_container(const std::string& container_id, const pid_t& container_pid, const std::string& container_name, const std::string& filesystem_path) {
    ContainerObject c;
    c.id = container_id;
    c.pid = container_pid;
    c.name = container_name.empty() ? "quiver-" + container_id.substr(0, 12) : container_name;
    c.image = filesystem_path.substr(filesystem_path.find_last_of("/") + 1);
    c.status = "running";
    c.hostname = container_id.substr(0, 6);
    c.filesystem_path = filesystem_path;
    c.pty_shell = "/bin/sh";  // TODO: Need to update later based on actual shell used

    // 3. Add the container to the database
    if (!m_db.add_container(c)) {
        return "";
    }

    return container_id;
}

// Retrieves full container information from the database
ContainerObject ContainerManager::get_container_info(const std::string& container_id) {
    return m_db.get_container(container_id);
}

// Lists all containers stored in the database
std::vector<ContainerObject> ContainerManager::list_all_containers() {
    return m_db.list_all_containers();
}

// Removes a container and its associated data
bool ContainerManager::remove_container(const std::string& container_id) {
    // 1. Get and remove all associated volumes
    std::vector<VolumeObject> volumes = m_db.get_container_volumes(container_id);
    for (const auto& volume : volumes) {
        // Remove each volume entry by its ID
        if (!m_db.remove_volume(volume.id)) {
            // Log an error if removal fails, but continue to remove the container
            std::cerr << "Warning: Failed to remove volume ID " << volume.id
                      << " for container " << container_id << " from DB." << std::endl;
        } else {
            std::cout << "Successfully removed volume ID " << volume.id << " from DB." << std::endl;
        }
    }

    // 2. Remove the container itself
    return m_db.remove_container(container_id);
}
// Logs container data to the console
void ContainerManager::log_container_data(const std::string& container_id) {
    ContainerObject c = get_container_info(container_id);
    if (!c.id.empty()) {
        std::cout << "--- Container Info ---" << std::endl;
        std::cout << "ID: " << c.id << std::endl;
        std::cout << "Name: " << c.name << std::endl;
        std::cout << "Image: " << c.image << std::endl;
        std::cout << "PID: " << c.pid << std::endl;
        std::cout << "Status: " << c.status << std::endl;
        std::cout << "Created: " << c.created_at << std::endl;
        std::cout << "----------------------" << std::endl;
    } else {
        std::cerr << "Could not find container with ID: " << container_id << std::endl;
    }
}


// Generates a unique SHA256 ID for a new container

