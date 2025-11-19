#include "../include/image_manager.hpp"
#include "../include/database_manager.hpp"
#include "../include/container_management.hpp"
#include "../include/utils.hpp"
#include "../include/command_line_handler.hpp"
#include <cstdlib>
#include <functional>
#include <string>
#include <memory>
#include <iostream>
#include <map>
#include <vector>

using CommandHandler = std::function<void(DatabaseManager&, ImageManager&, const std::vector<std::string>&)>;

int main(int argc, char* argv[]) {
    try {
        std::string base_dir{ Utils::get_base_dir() };
        Utils::ensure_dirs(base_dir);

        std::string db_path{ base_dir + "quiver.db" };
        auto db_ptr = std::make_unique<DatabaseManager>(db_path);
        if (!db_ptr->init_db()) {
            std::cerr << "Failed to initialize the database. Exiting." << '\n';
            return EXIT_FAILURE;
        }

        DatabaseManager& db{ *db_ptr };
        ImageManager img_manager(db);
        ContainerManager containerManager(db);

        if (argc < 2) {
            Utils::print_usage();
            return EXIT_FAILURE;
        }

        std::map<std::string, CommandHandler> commands{};
        commands["run"]    = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::run(db, args); };
        commands["attach"] = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::attach(db, args); };
        commands["ps"]     = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::ps(db, args); };
        commands["rm"]     = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::rm(db, args); };
        commands["start"]  = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::start(db, args); };
        commands["stop"]   = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::stop(db, args); };
        commands["image"]  = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::image(db, img, args); };
        commands["volume"] = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::volume(db, args); };
        commands["create"] = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::create(db, img, args); };
        commands["pull"]   = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ CommandLineHandler::pull(db, args); };
        commands["help"]   = [](DatabaseManager& db[[maybe_unused]],
                                ImageManager& img[[maybe_unused]],
                                const std::vector<std::string>& args[[maybe_unused]]){ Utils::print_usage(); };

        std::vector<std::string> cmds;
        for (int i = 2; i < argc; ++i) {
            cmds.emplace_back(argv[i]);
        }

        auto it = commands.find(argv[1]);
        if (it == commands.end()) {
            Utils::print_usage();
            return EXIT_FAILURE;
        } else {
            it->second(db, img_manager, cmds);
        }
        return EXIT_SUCCESS;
    } catch (const std::runtime_error& e) {
        std::cerr << "Database Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
