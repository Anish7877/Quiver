#include <ios>
#include <iostream>
#include <vector>
#include <string>
#include <span>
#include <exception>

#include "command_line_handler.hpp"
#include "utils.hpp"

void print_help() {
    std::cout << "Quiver CLI - Container Runtime\n\n"
              << "Usage: quiver <command> [OPTIONS]\n\n"
              << "Commands:\n"
              << "  run       Run a command in a new container\n"
              << "  ps        List containers\n"
              << "  help      Show this help message\n\n"
              << "Run 'quiver run --help' for more information on run options.\n";
}

int main(int argc, char* argv[]) {
    // If no arguments are passed, show the help mene
        std::ios::sync_with_stdio(false);
        std::cin.tie(NULL);
    if (argc < 2) {
        print_help();
        return 1;
    }
    // The first argument is the program name (argv[0]).
    // The second argument is the subcommand (e.g., 'run', 'ps').
    std::string command = argv[1];

    // Instantiate the handler

    try {
        pid_t consumer_pid{Utils::spawn_new_consumer()};
        if (command == "run") {
            // Collect all arguments AFTER 'run' to pass to the handler
            std::vector<std::string> args;

            // Start at index 2 to skip argv[0] (program name) and argv[1] ("run")
            for (int i = 2; i < argc; ++i) {
                args.push_back(argv[i]);
            }

            // Convert vector to std::span and execute
            CommandLineHandler::run(std::span<std::string>(args));

        } else if (command == "ps") {
                CommandLineHandler::ps();

        } else if (command == "rm") {
                CommandLineHandler::remove(argv[2]);
        } else if (command == "inspect") {
                CommandLineHandler::inspect(argv[2]);
        } else if (command == "help" || command == "--help" || command == "-h") {
            print_help();

        } else {
            std::cerr << "quiver: '" << command << "' is not a quiver command.\n"
                      << "See 'quiver --help'.\n";
            return 1;
        }
    } catch (const std::exception& e) {
        // Catch the std::runtime_error thrown by CommandLineHandler::run
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "An unknown fatal error occurred.\n";
        return 1;
    }

    return 0;
}
