#include <string>
#include "../include/image_management.hpp"
#include "../include/container.hpp"
#include "../include/database_manager.hpp" 
#include "../include/container_management.hpp"
#include "../include/utils.hpp"         

// Use a pointer to manage lifetime outside the try block
DatabaseManager* db_ptr = nullptr;
std::string db_path;

int main(int argc,char* argv[]) {
    try {
        std::string base_dir = Utils::get_base_dir();
        Utils::ensure_dirs(base_dir);
        
        db_path = Utils::get_base_dir() + "quiver.db";
        db_ptr = new DatabaseManager(db_path);
        if (!db_ptr->init_db()) {
            std::cerr << "Failed to initialize the database. Exiting." << std::endl;
            delete db_ptr;
            return 1;
        }
    } catch (const std::runtime_error& e) {
        std::cerr << "Database Error: " << e.what() << std::endl;
        return 1;
    }

    DatabaseManager& db = *db_ptr;

    ImageManager image{};
    std::string filesystem_path{};
    std::string err{};
    std::vector<std::string> volumes{};

    ContainerManager containerManager(db);
    std::string container_id{};

    if(argc > 1 && strcmp(argv[1],"attach") != 0 && image.pull(argv[1],filesystem_path,err)){
        std::string image_name = argv[1];
        
        // Create DB record before starting container
        container_id = containerManager.create_container(image_name);
        if (container_id.empty()) {
            std::cerr << "Failed to create container record in DB." << std::endl;
            delete db_ptr; return 1;
        }

        Container ctr{ "container", filesystem_path, volumes, db, container_id };
        ctr.set_filesystem(filesystem_path);
        
        ctr.exec("/bin/bash");
    }
    else if(argc > 1 && strcmp(argv[1],"attach") == 0){
        pid_t container_pid{ (pid_t)std::stoi(argv[2]) };
        Container attach_ctr; 
        attach_ctr.connect_to_server(container_pid);
    }

    delete db_ptr;
    return 0;
}