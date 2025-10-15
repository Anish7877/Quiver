#include "../include/image_management.hpp"
#include "../include/container.hpp"
#include "../include/database_manager.hpp"
#include "../include/container_management.hpp"
#include "../include/utils.hpp"
#include "../include/command_line_handler.hpp"
#include <cstdlib>
#include <functional>
#include <string>
#include <memory>

using CommandHandler = std::function<void(DatabaseManager&, ImageManager&, const std::vector<std::string>&)>;

int main(int argc, char* argv[]) {
    try {
        std::string base_dir = Utils::get_base_dir();
        Utils::ensure_dirs(base_dir);

        std::string db_path = base_dir + "quiver.db";
        auto db_ptr = std::make_unique<DatabaseManager>(db_path);
        if (!db_ptr->init_db()) {
            std::cerr << "Failed to initialize the database. Exiting." << '\n';
            return EXIT_FAILURE;
        }

        DatabaseManager& db = *db_ptr;
        ImageManager img_manager;
        ContainerManager containerManager(db);

        if (argc < 2) {
            Utils::print_usage();
            return EXIT_FAILURE;
        }

        std::map<std::string, CommandHandler> commands{};
        commands["run"]    = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::run(db, img, args); };
        commands["attach"] = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::attach(args); };
        commands["ps"]     = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::ps(db, args); };
        commands["rm"]     = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::rm(db, args); };
        commands["start"]  = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::start(db, args); };
        commands["stop"]   = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::stop(db, args); };
        commands["image"]  = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::image(db, img, args); };
        commands["volume"] = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::volume(db, args); };
        commands["create"] = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::create(db, img, args); };
        commands["pull"]   = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ CommandLineHandler::pull(db, img, args); };
        commands["help"]  = [](DatabaseManager& db, ImageManager& img, const std::vector<std::string>& args){ Utils::print_usage(); };

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