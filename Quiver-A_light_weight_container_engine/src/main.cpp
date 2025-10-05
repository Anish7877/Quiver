#include "../include/process.hpp"
#include "../include/tty_proxy_server.hpp"
#include "../include/image_management.hpp"
#include "../include/orchestrator.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <csignal>
#include <cstring>

// Global pointer to the orchestrator, used by the signal handler for graceful shutdown.
Orchestrator* g_orchestrator = nullptr;

// A handler to catch signals like Ctrl+C (SIGINT) to shut down gracefully.
void signal_handler(int signum) {
    std::cout << "\nCaught signal " << signum << ". Shutting down..." << std::endl;
    if (g_orchestrator) {
        g_orchestrator->shutdown(); // Call the orchestrator's shutdown method.
    }
    exit(signum); // Exit the program.
}

int main(int argc, char* argv[]) {
    // --- START: ADDED CODE FOR .QIVR FUNCTIONALITY ---
    // This block checks for the new 'run' command. If found, it handles it
    // and exits, never touching the original code below.
    if (argc > 1 && strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            Utils::handle_error("Usage: quiver run <file.qivr>");
            return 1;
        }
        Orchestrator orchestrator;      // Create the main orchestrator object.
        g_orchestrator = &orchestrator; // Assign its address to the global pointer for the signal handler.

        // Setup signal handling to catch termination signals.
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        // Run the orchestrator and return its exit code, exiting the program here.
        return orchestrator.run(argv[2]);
    }
    // --- END: ADDED CODE ---

    /*
     * ===================================================================
     * |                                                                 |
     * |         YOUR ORIGINAL, UNCHANGED CODE STARTS HERE               |
     * |                                                                 |
     * ===================================================================
     */
    ImageManager image{};
    std::string filesystem_path{};
    std::string err{};
    TTYProxyServer tty{};
    std::vector<std::string> volumes{};
    if (argc > 1 && strcmp(argv[1], "attach") != 0 && image.pull(argv[1], filesystem_path, err)) {
        Process p{};
        if (p.start("container", volumes, filesystem_path, "/bin/sh") == -1) {
            _exit(0);
        }
    } else if (argc > 1 && strcmp(argv[1], "attach") == 0) {
        tty.reattach_to_socket(argv[2]);
    }
    /*
     * ===================================================================
     * |                                                                 |
     * |          YOUR ORIGINAL, UNCHANGED CODE ENDS HERE                |
     * |                                                                 |
     * ===================================================================
     */

    return 0;
}