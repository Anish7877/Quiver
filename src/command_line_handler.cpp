#include "../include/utils.hpp"
#include "../include/database_manager.hpp"
#include "../include/image_management.hpp"
#include "../include/command_line_handler.hpp"
#include <vector>
#include <iostream>

std::vector<std::string> volumes{};
std::vector<std::string> commands{};
std::string image_name{""};
std::string container_name{""};
pid_t container_pid{-1};


void CommandLineHandler::run(const std::vector<std::string>& cmds){
    size_t i{1};
    if(cmds[0] == "-v"){
        while(i < cmds.size() && cmds[i] != "-i"){
            volumes.emplace_back(cmds[i]);
            ++i;
        }
        ++i;
    }
    if(i < cmds.size()){
        image_name = cmds[i++];
    }
    else{
        Utils::print_usage();
    }
    while(i < cmds.size()){
        commands.emplace_back(cmds[i]);
    }
}

void CommandLineHandler::attach(const std::vector<std::string>& cmds){
    if(cmds.size() > 2){
        Utils::print_usage();
    }
    container_pid = std::stoi(cmds[1]);
}

void CommandLineHandler::ps(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    std::vector<ContainerObject> containers{};
    if(cmds.size() == 0){
        // To do list all running container table of database in table form
        containers = db_manager.list_running_containers();
    }
    else if(cmds.size() == 1 && cmds[0] == "-a"){
        // To do list all the containers in database
        containers = db_manager.list_all_containers();
    }

    // Print the containers in a tabular format
    std::cout << "CONTAINER ID\tNAME\tIMAGE\tPID\tSTATUS\tCREATED AT\tHOSTNAME\tFILESYSTEM PATH\tPTY SHELL\n";
    for (const auto& container : containers) {
        std::cout << container.id << "\t"
            << container.name << "\t"
            << container.image << "\t"
            << container.pid << "\t" 
            << container.status << "\t"
            << container.created_at << "\t"
            << container.hostname << "\t"
            << container.filesystem_path << "\t"
            << container.pty_shell << "\n";
    }
}

void CommandLineHandler::rm(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() > 2){
        Utils::print_usage();
    }
    container_name = cmds[1];
    
    // remove db entry from database of container
    if (db_manager.remove_container(container_name)) {
        std::cout << "Container " << container_name << " removed successfully from database.\n";
    } else {
        std::cout << "Failed to remove container " << container_name << " from database.\n";
    }
}

void CommandLineHandler::image(DatabaseManager& db_manager, ImageManager& img_manager, const std::vector<std::string>&  cmds){
    if (cmds.size() != 1) {
        Utils::print_usage();
    }

    if(cmds[0] == "ls"){
        // list all the images downloaded and their versions
        std::vector<ImageObject> images = db_manager.list_all_images();

        // Print the images in a tabular format
        std::cout << "IMAGE ID\tNAME\tTAG\tCREATED AT\n";
        for (const auto& image : images) {
            std::cout << image.id << "\t"
                << image.name << "\t"
                << image.tag << "\t"
                << image.created_at << "\n";
        }
    }
    else if(cmds[0] == "rm"){
        image_name = cmds[1];

        // remove image from the local storage
        std::string err;
        if (img_manager.remove(image_name, err)) {
            std::cout << "Image " << image_name << " removed successfully from local storage.\n";
        } else {
            std::cout << "Failed to remove image " << image_name << " from local storage.\n";
        }
        db_manager.remove_image(image_name);

    }
    else if(cmds[0] == "cls"){
        if(cmds.size() > 1){
            image_name = cmds[1];
            // list containers which are using a particular image
            std::vector<ContainerObject> containers = db_manager.list_containers_by_image(image_name);

            // Print the containers in a tabular format
            std::cout << "CONTAINER ID\tNAME\tIMAGE\tPID\tSTATUS\tCREATED AT\tHOSTNAME\tFILESYSTEM PATH\tPTY SHELL\n";
            for (const auto& container : containers) {
                std::cout << container.id << "\t"
                    << container.name << "\t"
                    << container.image << "\t"
                    << container.pid << "\t" 
                    << container.status << "\t"
                    << container.created_at << "\t"
                    << container.hostname << "\t"
                    << container.filesystem_path << "\t"
                    << container.pty_shell << "\n";
            }
        }
        else{
            Utils::print_usage();
        }
    }
}

void CommandLineHandler::volume(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds[0] == "ls"){
        // list all the volumes database in tabular form
        std::vector<VolumeObject> volumes = db_manager.list_all_volumes();
        std::cout << "VOLUME ID\tCONTAINER ID\tHOST PATH\tCONTAINER PATH\n";
        for (const auto& volume : volumes) {
            std::cout << volume.id << "\t"
                << volume.container_name << "\t"
                << volume.host_path << "\t"
                << volume.container_path << "\n";
        }
    }
    // baaki abhi not sure
}

void CommandLineHandler::create(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds[0] == "container"){
        std::vector<std::string> new_cmds{ cmds };
        new_cmds.erase(new_cmds.begin());
        run(new_cmds);
    }
    else if(cmds[0] == "volume"){
        // write to database for further volume details
        int i = 1;
        while (true) {
            std::string volume_input = cmds[i];
            size_t sep_pos = volume_input.find(':');
            if (sep_pos == std::string::npos) {
                std::cerr << "Invalid volume format. Expected host_path:container_path\n";
                break;
            }
            std::string host_path = volume_input.substr(0, sep_pos);
            std::string container_path = volume_input.substr(sep_pos + 1);
            
            VolumeObject volume;
            volume.host_path = host_path;
            volume.container_path = container_path;

            // Add volume to database
            db_manager.add_volume(volume);

            ++i;
            if (i >= cmds.size()) break;
        }
    }
}

void CommandLineHandler::pull(DatabaseManager& db_manager, ImageManager& img_manager, const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    image_name = cmds[0];

    // pull image and save to local storage
    std::string out_path="";
    std::string err="";
    img_manager.pull(image_name, out_path, err);
    if (!err.empty()) {
        std::cout << "Failed to pull image: " << err << std::endl;
    } else {
        std::cout << "Image pulled successfully to: " << out_path << std::endl;
        if (db_manager.add_image(image_name)) {
            std::cout << "Image " << image_name << " added to database successfully.\n";
        } else {
            std::cout << "Failed to add image " << image_name << " to database.\n";
        }
    }
}

void CommandLineHandler::start(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    container_name = cmds[0];
    // start the stopped container with loaded info
    if (db_manager.update_container_status(cmds[0], "running")) {
        std::cout << "Container " << cmds[0] << " running successfully.\n";
    } else {
        std::cout << "Failed to update status of " << cmds[0] << " to RUNNING.\n";
    }
}

void CommandLineHandler::stop(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    // stop the container shell and make running status false in database
    if (db_manager.update_container_status(cmds[0], "stopped")) {
        std::cout << "Container " << cmds[0] << " stopped successfully.\n";
    } else {
        std::cout << "Failed to stop container " << cmds[0] << ".\n";
    }
}
