#include "../include/orchestrator.hpp"
#include "../include/qivr_parser.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <functional> // Required for std::function

// Constructor for the Orchestrator class.
Orchestrator::Orchestrator() {}

// Destructor ensures that shutdown is called to clean up resources.
Orchestrator::~Orchestrator() {
    shutdown();
}

// Main function to execute the application defined in the .qivr file.
int Orchestrator::run(const std::string& qivr_path) {
    // Enforce the .qivr file extension for clarity and consistency.
    std::string extension = ".qivr";
    if (qivr_path.length() < extension.length() ||
        qivr_path.substr(qivr_path.length() - extension.length()) != extension) {
        Utils::handle_error("Invalid file provided. The file must have a '.qivr' extension.");
        return -1;
    }

    // Create a struct to hold the parsed application definition.
    AppDefinition app_def;
    if (!parse_and_validate(qivr_path, app_def)) {
        return -1; // Exit if parsing or validation fails.
    }

    // Determine the correct order to start services based on dependencies.
    std::vector<std::string> startup_order;
    try {
        startup_order = get_startup_order(app_def);
    } catch (const std::runtime_error& e) {
        Utils::handle_error(e.what()); // Handle errors like circular dependencies.
        return -1;
    }

    std::cout << "Starting application..." << '\n';
    // Loop through the services in the calculated startup order.
    for (const auto& service_name : startup_order) {
        const auto& service_config = app_def.services[service_name];
        std::cout << "--- Starting service: " << service_name << " ---" << '\n';

        // Pull the required image using your existing ImageManager.
        std::string image_path, error;
        if (!m_image_manager.pull(service_config.image, image_path, error)) {
            Utils::handle_error("Failed to pull image " + service_config.image + ": " + error);
            return -1;
        }

        // Create and start the container using your existing Process class.
        Process p;
        if (p.start(service_name, service_config.volumes, image_path, "/bin/sh") == -1) {
            Utils::handle_error("Failed to start container for service " + service_name);
            shutdown(); // Clean up any services that were already started.
            return -1;
        }

        // Store the PID of the new container for tracking.
        pid_t container_pid = p.pid();
        m_running_containers[service_name] = container_pid;

        // Setup networking, with or without ports, using your existing Network class.
        if (!service_config.ports.empty()) {
            if (Network::setup_networking_with_ports(container_pid, service_config.ports) != 0) {
                Utils::handle_error("Failed to set up networking with ports for " + service_name);
                shutdown();
                return -1;
            }
        } else {
             if (Network::setup_networking(container_pid) != 0) {
                Utils::handle_error("Failed to set up networking for " + service_name);
                shutdown();
                return -1;
            }
        }

        std::cout << "Service " << service_name << " started successfully with PID: " << container_pid << '\n';
    }

    std::cout << "\nApplication started successfully. Press Ctrl+C to shut down." << '\n';
    // Keep the main process alive while the containers run in the background.
    while (true) {
        sleep(1);
    }

    return 0;
}

// Gracefully shuts down all running containers.
void Orchestrator::shutdown() {
    std::cout << "\nShutting down all services..." << '\n';
    // Iterate over all tracked containers.
    for (auto const& [name, pid] : m_running_containers) {
        std::cout << "Stopping container for service: " << name << " (PID: " << pid << ")" << '\n';
        kill(pid, SIGTERM); // Send a termination signal to the container process.
        Network::cleanup_networking(pid); // Clean up the container's network resources.
    }
    m_running_containers.clear(); // Clear the map of running containers.
    std::cout << "Shutdown complete." << '\n';
}

// Private helper to parse the .qivr file and perform basic validation.
bool Orchestrator::parse_and_validate(const std::string& qivr_path, AppDefinition& app_def) {
    try {
        QivrParser parser; // Create an instance of our YAML parser.
        app_def = parser.parse(qivr_path); // Parse the file into our C++ structs.

        // Validate that there is at least one service defined.
        if (app_def.services.empty()) {
            throw std::runtime_error("No services defined in .qivr file.");
        }

        // Validate that every service has an image specified.
        for (const auto& pair : app_def.services) {
            if (pair.second.image.empty()) {
                throw std::runtime_error("Service '" + pair.first + "' is missing an 'image' field.");
            }
        }
        return true; // Return true if parsing and validation succeed.

    } catch (const std::exception& e) {
        Utils::handle_error("Failed to parse .qivr file: " + std::string(e.what()));
        return false; // Return false on any parsing error.
    }
}

// Calculates the service startup order using a topological sort algorithm.
std::vector<std::string> Orchestrator::get_startup_order(const AppDefinition& app_def) {
    std::vector<std::string> sorted_order;            // The final, linear list of services to start.
    std::map<std::string, bool> visited;              // Tracks services that have been visited.
    std::map<std::string, bool> recursion_stack;      // Tracks services in the current recursion path to detect cycles.

    // A recursive helper function to perform the Depth First Search.
    std::function<void(const std::string&)> visit =
        [&](const std::string& service_name) {
        visited[service_name] = true;
        recursion_stack[service_name] = true;

        // Visit all dependencies of the current service.
        const auto& service_config = app_def.services.at(service_name);
        for (const auto& dep : service_config.depends_on) {
            // Check if a dependency points to a service that doesn't exist.
            if (app_def.services.find(dep) == app_def.services.end()) {
                throw std::runtime_error("Service '" + service_name + "' depends on undefined service '" + dep + "'.");
            }
            // If the dependency is already in the recursion stack, we have a circular dependency.
            if (recursion_stack[dep]) {
                throw std::runtime_error("Circular dependency detected involving service '" + service_name + "'.");
            }
            // If the dependency hasn't been visited yet, visit it.
            if (!visited[dep]) {
                visit(dep);
            }
        }

        recursion_stack[service_name] = false; // Remove service from recursion stack.
        sorted_order.push_back(service_name);  // Add the service to the final sorted list.
    };

    // Iterate through all services to ensure all are visited (handles disconnected graphs).
    for (const auto& pair : app_def.services) {
        if (!visited[pair.first]) {
            visit(pair.first);
        }
    }

    return sorted_order;
}