#include "../include/image_management.hpp"
#include "../include/container.hpp"
#include <string>
#include "../include/database_manager.hpp" 
#include "../include/utils.hpp"         

int main(int argc,char* argv[]) {
    try {
        std::string base_dir = Utils::get_base_dir();
        Utils::ensure_dirs(base_dir);
        
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
    std::vector<std::string> volumes{};
    Container ctr{ "container", filesystem_path, volumes };
    if(argc > 1 && strcmp(argv[1],"attach") != 0 && image.pull(argv[1],filesystem_path,err)){
        ctr.set_filesystem(filesystem_path);
        ctr.exec("/bin/bash");
    }
    else if(argc > 1 && strcmp(argv[1],"attach") == 0){
        pid_t container_pid{ (pid_t)std::stoi(argv[2]) };
        ctr.connect_to_server(container_pid);
    }
    return 0;
}