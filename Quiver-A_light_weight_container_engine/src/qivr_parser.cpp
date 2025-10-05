#include "../include/qivr_parser.hpp"
#include <yaml-cpp/yaml.h> // The header for the yaml-cpp library.
#include <fstream>
#include <stdexcept>

// Parses the given file path and returns a structured AppDefinition.
AppDefinition QivrParser::parse(const std::string& file_path) {
    // Open the file for reading.
    std::ifstream fin(file_path);
    if (!fin) {
        throw std::runtime_error("Cannot open .qivr file: " + file_path);
    }

    // Load and parse the YAML file content.
    YAML::Node config = YAML::Load(fin);
    AppDefinition app_def; // The C++ object to populate.

    // Ensure the top-level 'services' key exists.
    if (!config["services"]) {
        throw std::runtime_error("'.qivr' file must contain a 'services' section.");
    }

    // Iterate over each service defined under the 'services' key.
    for (const auto& service_node : config["services"]) {
        std::string service_name = service_node.first.as<std::string>(); // Get the service name (e.g., "database").
        YAML::Node service_data = service_node.second;                   // Get the configuration data for that service.
        ServiceConfig service_config;                                    // Create a C++ struct for the service.
        service_config.name = service_name;

        // Safely access and assign values from the YAML node to the C++ struct.
        if (service_data["image"]) {
            service_config.image = service_data["image"].as<std::string>();
        }
        if (service_data["command"]) {
            service_config.command = service_data["command"].as<std::string>();
        }
        if (service_data["volumes"]) {
            for (const auto& volume_node : service_data["volumes"]) {
                service_config.volumes.push_back(volume_node.as<std::string>());
            }
        }
        if (service_data["ports"]) {
            for (const auto& port_node : service_data["ports"]) {
                std::string port_map = port_node.as<std::string>();
                size_t colon_pos = port_map.find(':'); // Find the separator for host:container port.
                if (colon_pos == std::string::npos) {
                    throw std::runtime_error("Invalid port mapping in service '" + service_name + "': " + port_map);
                }
                // Convert port strings to integers.
                int host_port = std::stoi(port_map.substr(0, colon_pos));
                int container_port = std::stoi(port_map.substr(colon_pos + 1));
                service_config.ports.push_back({host_port, container_port});
            }
        }
        if (service_data["depends_on"]) {
            for (const auto& dep_node : service_data["depends_on"]) {
                service_config.depends_on.push_back(dep_node.as<std::string>());
            }
        }

        // Add the fully populated service config to the main application definition.
        app_def.services[service_name] = service_config;
    }

    return app_def; // Return the complete C++ representation of the .qivr file.
}