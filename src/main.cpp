#include "../include/process.hpp"
#include "../include/tty_proxy_server.hpp"
#include "../include/image_management.hpp"
#include "../include/container_management.hpp"
#include <iostream>
#include <cstring>
#include <vector>
#include <iomanip>

// Helper function to print a formatted table of containers.
void print_containers(const std::vector<Container>& containers) {
    if (containers.empty()) {
        std::cout << "No containers found.\n";
        return;
    }
    // Print table headers.
    std::cout << std::left
              << std::setw(15) << "CONTAINER ID"
              << std::setw(20) << "IMAGE"
              << std::setw(15) << "STATUS"
              << std::setw(25) << "NAME" << "\n";

    // Print each container's information.
    for (const auto& c : containers) {
        std::cout << std::left
                  << std::setw(15) << c.id.substr(0, 12)
                  << std::setw(20) << c.image
                  << std::setw(15) << c.status
                  << std::setw(25) << c.name << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: quiver <command> [args...]\n";
        return 1;
    }

    ContainerManager container_manager;
    std::string error;

    // Handle the 'run' command.
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            std::cerr << "Usage: quiver run <image_name>\n";
            return 1;
        }
        ImageManager image_manager;
        std::string filesystem_path;
        if (image_manager.pull(argv[2], filesystem_path, error)) {
            std::string container_id = container_manager.create_container(argv[2], error);
            if (container_id.empty()) {
                std::cerr << "Failed to create container record: " << error << "\n";
                return 1;
            }
            std::cout << "Created container: " << container_id << "\n";

            Process p{};
            // The start method now returns the PID of the forked monitor process.
            pid_t container_pid = p.start("container", filesystem_path, "/bin/bash");
            if (container_pid != -1) {
                // Update the database with the PID and "running" status.
                if (!container_manager.start_container(container_id, container_pid, error)) {
                    std::cerr << "Failed to update container status: " << error << "\n";
                }
            } else {
                std::cerr << "Failed to start container process.\n";
            }
        } else {
            std::cerr << "Failed to pull image: " << error << "\n";
            return 1;
        }
    // Handle the 'ps' command to list containers.
    } else if (strcmp(argv[1], "ps") == 0) {
        std::vector<Container> containers = container_manager.list_containers(error);
        if (!error.empty()) {
            std::cerr << "Failed to list containers: " << error << "\n";
            return 1;
        }
        print_containers(containers);
    // Handle the 'attach' command.
    } else if (strcmp(argv[1], "attach") == 0) {
        if (argc < 3) {
            std::cerr << "Usage: quiver attach <container_id_or_name>\n";
            return 1;
        }
        // Use the new function to find the container's PID from the database.
        pid_t pid = container_manager.get_container_pid(argv[2], error);
        if (pid != -1) {
            TTYProxyServer tty{};
            // Construct the socket path from the PID.
            std::string socket_path = tty.get_sock_path(pid);
            std::cout << "Attaching to container " << argv[2] << " (socket: " << socket_path << ")\n";
            // Connect to the TTY socket.
            tty.reattach_to_socket(socket_path);
        } else {
            std::cerr << "Error: " << error << "\n";
            return 1;
        }
    } else {
        std::cerr << "Unknown command: " << argv[1] << "\n";
        return 1;
    }

    return 0;
}
