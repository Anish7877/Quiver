#pragma once

#include "image_management.hpp"
#include "process.hpp"
#include "network.hpp"
#include "volumes.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>

// Represents the configuration for a single service from the .qivr file.
struct ServiceConfig {
    std::string name;                                   // Name of the service (e.g., "database").
    std::string image;                                  // Image to use (e.g., "postgres:14-alpine").
    std::string command;                                // Command to run inside the container (future use).
    std::vector<std::string> volumes;                   // Volume mappings (e.g., "db-data:/var/lib/data").
    std::vector<std::pair<int, int>> ports;             // Port mappings {host_port, container_port}.
    std::vector<std::string> depends_on;                // List of services this one depends on.
};

// Represents the entire application defined in a .qivr file.
struct AppDefinition {
    std::map<std::string, ServiceConfig> services;      // A map of all services defined in the file.
    std::vector<std::string> networks;                  // Reserved for future multi-network features.
    std::vector<std::string> volumes;                   // Reserved for future named volume features.
};

// The main class responsible for managing the container lifecycle based on a .qivr file.
class Orchestrator {
public:
    // Constructor: Initializes a new Orchestrator instance.
    Orchestrator();
    // Destructor: Ensures graceful shutdown of all services.
    ~Orchestrator();

    // The main entry point to run the application from a .qivr file.
    int run(const std::string& qivr_path);

    // Stops and cleans up all running containers.
    void shutdown();

private:
    // Parses and validates the contents of the .qivr file.
    bool parse_and_validate(const std::string& qivr_path, AppDefinition& app_def);

    // Determines the correct startup order based on service dependencies.
    std::vector<std::string> get_startup_order(const AppDefinition& app_def);

    // --- Core Component Managers ---
    ImageManager m_image_manager;                       // An instance of your existing ImageManager.

    // --- State Tracking ---
    std::map<std::string, pid_t> m_running_containers;  // Tracks running containers by mapping service name to PID.
};