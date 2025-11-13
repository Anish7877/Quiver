#include "../include/utils.hpp"
#include "../include/database_manager.hpp"
#include "../include/image_manager.hpp"
#include "../include/command_line_handler.hpp"
#include "../include/container.hpp"
#include <cstdlib>
#include <vector>
#include <iostream>

std::vector<std::string> volumes{};
std::vector<std::string> commands{};
std::string image_name{""};
std::string container_name{""};
pid_t container_pid{-1};
std::string root_fs{""};
std::string err{""};


void CommandLineHandler::run(DatabaseManager& db, const std::vector<std::string>& cmds){
    if(cmds.size() == 0){
        Utils::print_usage();
        return;
    }
    size_t i{1};
    if(cmds[0] == "-v"){
        while(i < cmds.size() && cmds[i] != "-i"){
            volumes.emplace_back(cmds[i]);
            ++i;
        }
    }
    if((cmds[i] == "-i" || cmds[0] == "-i") && i+1 < cmds.size()){
        ++i;
        image_name = cmds[i++];
    }
    else{
        Utils::print_usage();
    }
    while(i < cmds.size()){
        commands.emplace_back(cmds[i]);
        ++i;
    }

    CommandLineHandler::pull(db, {image_name});

    Container container("container", root_fs, volumes, db, Utils::generate_container_id());
    container.exec("/bin/bash", commands);
}

void CommandLineHandler::attach(const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
        return;
    }
    container_pid = std::stoi(cmds[0]);

    Container container;
    container.connect_to_server(container_pid);
}

void CommandLineHandler::ps(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
        return;
    }
    std::vector<ContainerObject> containers{};
    if(cmds.size() == 0){
        containers = db_manager.list_running_containers();
    }
    else if(cmds.size() == 1 && cmds[0] == "-a"){
        containers = db_manager.list_all_containers();
    }

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
    if(cmds.size() != 1){
        Utils::print_usage();
    }
    container_name = cmds[0];

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
        std::vector<ImageObject> images = db_manager.list_all_images();

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
    else{
        Utils::print_usage();
    }
}

void CommandLineHandler::volume(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds[0] == "ls"){
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

void CommandLineHandler::create(DatabaseManager& db_manager, ImageManager& img, const std::vector<std::string>& cmds){
    if(cmds[0] == "container"){
        std::vector<std::string> new_cmds{ cmds };
        new_cmds.erase(new_cmds.begin());
        run(db_manager, new_cmds);
    }
    else if(cmds[0] == "volume"){
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

            db_manager.add_volume(volume);

            ++i;
            if (i >= cmds.size()) break;
        }
    }
}

void CommandLineHandler::pull(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    image_name = cmds[0];

    ImageManager img_manager(db_manager);
    std::string out_path;
    std::string err;
    if (!img_manager.pull(image_name, out_path, err)) {
        std::cout << "Failed to pull image: " << err << std::endl;
        exit(EXIT_FAILURE);
    } else {
        std::cout << "Image pulled successfully to: " << out_path << std::endl;
    }
    root_fs = out_path;
}

void CommandLineHandler::start(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    container_name = cmds[0];
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
    if (db_manager.update_container_status(cmds[0], "stopped")) {
        std::cout << "Container " << cmds[0] << " stopped successfully.\n";
    } else {
        std::cout << "Failed to stop container " << cmds[0] << ".\n";
    }
}
