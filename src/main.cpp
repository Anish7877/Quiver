#include <ios>
#include <iostream>
#include <vector>
#include <string>
#include <span>
#include <exception>

#include "command_line_handler.hpp"
#include "utils.hpp"

int main(int argc, char* argv[]) {
        std::ios::sync_with_stdio(false);
        std::cin.tie(NULL);
        if (argc < 2) {
                return 1;
        }
        std::string command = argv[1];
        std::vector<std::string> args;

        for (int i = 2; i < argc; ++i) {
                args.push_back(argv[i]);
        }

        try {
                pid_t consumer_pid{Utils::spawn_new_consumer()};
                if (command == "run") {
                        CommandLineHandler::run(args);
                } else if (command == "ps") {
                        CommandLineHandler::ps(args);
                } else if (command == "rm") {
                        CommandLineHandler::remove(args);
                } else if (command == "inspect") {
                        CommandLineHandler::inspect(args);
                } else if (command == "pause") {
                        CommandLineHandler::pause(args);
                } else if (command == "unpause") {
                        CommandLineHandler::unpause(args);
                } else if (command == "attach") {
                        CommandLineHandler::attach(args);
                } else if (command == "ports") {
                        CommandLineHandler::ports(args);
                } else if (command == "start") {
                        CommandLineHandler::start(args);
                } else if (command == "stop") {
                        CommandLineHandler::stop(args);
                } else if (command == "prune") {
                        CommandLineHandler::prune(args);
                } else if (command == "cp") {
                        CommandLineHandler::cp(args);
                } else if (command == "stats") {
                        CommandLineHandler::stats(args);
                } else if (command == "generate-systemd") {
                        CommandLineHandler::generate_systemd(args);
                } else if (command == "top") {
                        CommandLineHandler::top(args);
                } else if (command == "update") {
                        CommandLineHandler::update(args);
                } else if (command == "build") {
                        CommandLineHandler::build(args);
                } else if (command == "create") {
                        CommandLineHandler::create(args);
                } else if (command == "image") {
                        CommandLineHandler::image(args);
                } else if (command == "restart") {
                        CommandLineHandler::restart(args);
                } else if (command == "mount") {
                        CommandLineHandler::mount(args);
                } else if (command == "exec") {
                        CommandLineHandler::exec(args);
                } else if (command == "wait") {
                        CommandLineHandler::wait(args);
                } else if (command == "kill") {
                        CommandLineHandler::kill(args);
                } else {
                        std::cerr << "quiver: '" << command << "' is not a quiver command.\n"
                                << "See 'quiver --help'.\n";
                        return 1;
                }
        } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << "\n";
                return 1;
        } catch (...) {
                std::cerr << "An unknown fatal error occurred.\n";
                return 1;
        }
        return 0;
}
