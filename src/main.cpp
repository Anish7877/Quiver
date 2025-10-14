#include "../include/image_management.hpp"
#include "../include/container.hpp"
#include "../include/database_manager.hpp"
#include "../include/container_management.hpp"
#include "../include/utils.hpp"
#include "../include/command_line_handler.hpp"
#include <cstdlib>
#include <functional>
#include <string>
using CommandHandler = std::function<void(const std::vector<std::string>&)>;

DatabaseManager* db_ptr = nullptr;
std::string db_path;

int main(int argc,char* argv[]) {
    try {
        std::string base_dir = Utils::get_base_dir();
        Utils::ensure_dirs(base_dir);

        db_path = Utils::get_base_dir() + "quiver.db";
        db_ptr = new DatabaseManager(db_path);
        if (!db_ptr->init_db()) {
            std::cerr << "Failed to initialize the database. Exiting." << '\n';
            delete db_ptr;
            return 1;
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Database Error: " << e.what() << '\n';
        return 1;
    }

    DatabaseManager& db = *db_ptr;

    ImageManager image{};
    std::string filesystem_path{};
    std::string err{};
    std::vector<std::string> volumes{};

    ContainerManager containerManager(db);
    std::string container_id{};

    if(argc < 2){
        Utils::print_usage();
        return EXIT_FAILURE;
    }
    std::map<std::string, CommandHandler> commands{};
    commands["run"] = [](const std::vector<std::string>& args){ CommandLineHandler::run(args); };
    commands["attach"] = [](const std::vector<std::string>& args){ CommandLineHandler::attach(args); };
    commands["ps"] = [](const std::vector<std::string>& args){ CommandLineHandler::ps(args); };
    commands["rm"] = [](const std::vector<std::string>& args){ CommandLineHandler::rm(args); };
    commands["start"] = [](const std::vector<std::string>& args){ CommandLineHandler::start(args); };
    commands["stop"] = [](const std::vector<std::string>& args){ CommandLineHandler::stop(args); };
    commands["image"] = [](const std::vector<std::string>& args){ CommandLineHandler::image(args); };
    commands["volume"] = [](const std::vector<std::string>& args){ CommandLineHandler::volume(args); };
    commands["create"] = [](const std::vector<std::string>& args){ CommandLineHandler::create(args); };
    commands["pull"] = [](const std::vector<std::string>& args){ CommandLineHandler::pull(args); };

    std::vector<std::string> cmds{};
    for(int i{2};i<argc;++i){
        cmds.emplace_back(argv[i]);
    }
    auto it{ commands.find(argv[1]) };
    if(it == commands.end()){
        Utils::print_usage();
    }
    else{
        it->second(cmds);
    }
    delete db_ptr;
    return EXIT_SUCCESS;
}
