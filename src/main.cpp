#include "../include/process.hpp"
#include "../include/tty_proxy_server.hpp"
#include "../include/image_management.hpp"
#include "../include/database_manager.hpp" // 1. Include the DatabaseManager header
#include "../include/utils.hpp"          // 2. Include for utility functions

int main(int argc, char* argv[]) {
    // 3. Initialize the DatabaseManager
    try {
        std::string db_path = Utils::get_base_dir() + "quiver.db";
        DatabaseManager db(db_path);
        if (!db.init_db()) {
            std::cerr << "Failed to initialize the database. Exiting." << std::endl;
            return 1;
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Database Error: " << e.what() << std::endl;
        return 1;
    }

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

    return 0;
}