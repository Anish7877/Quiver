#include "../include/utils.hpp"
#include "../include/database_manager.hpp"
#include "../include/image_manager.hpp"
#include "../include/command_line_handler.hpp"
#include "../include/container.hpp"
#include <cstdlib>
#include <vector>
#include <iostream>

std::vector<VolumeObject> volumes{};
std::vector<std::string> commands{};
std::vector<std::pair<int,int>> forward_ports{};
std::string image_name{""};
std::string container_id{""};
std::string container_name{""};
pid_t container_pid{-1};
std::string root_fs{""};
std::string err{""};
bool vfs{false};
bool no_remove{false};


void CommandLineHandler::run(DatabaseManager& db, const std::vector<std::string>& cmds){
    if (cmds.empty()) {
        Utils::print_usage();
        return;
    }
    size_t i{ 0 };
    while (i < cmds.size()) {
        const std::string& arg { cmds[i] };

        if (arg == "-v" || arg == "--volume") {
            if (i + 1 < cmds.size()) {
                std::string vol_str{ cmds[++i] };
                size_t pos{ vol_str.find(':') };
                if (pos != std::string::npos) {
                    try {
                        std::string host{ vol_str.substr(0, pos) };
                        std::string cont{ vol_str.substr(pos + 1) };
                        VolumeObject vol{};
                        vol.host_path = host;
                        vol.container_path = cont;
                        volumes.emplace_back(vol);
                    } catch (...) {
                        std::cerr << "Error: Invalid port format " << vol_str << "\n";
                        return;
                    }
                }
            } else {
                std::cerr << "Error: -v or --volume requires an argument\n";
                return;
            }
        }
        else if (arg == "-p" || arg == "--port") {
            if (i + 1 < cmds.size()) {
                std::string port_str{ cmds[++i] };
                size_t pos{ port_str.find(':') };
                if (pos != std::string::npos) {
                    try {
                        int host{ std::stoi(port_str.substr(0, pos)) };
                        int cont{ std::stoi(port_str.substr(pos + 1)) };
                        forward_ports.emplace_back(host, cont);
                    } catch (...) {
                        std::cerr << "Error: Invalid port format " << port_str << "\n";
                        return;
                    }
                }
            } else {
                std::cerr << "Error: -p or --port requires an argument\n";
                return;
            }
        }
        else if(arg == "-n" || arg == "--name"){
            if(i+1 < cmds.size()){
                container_name = cmds[++i];
            }
            else{
                std::cerr << "Error: -n or --name requires an argument\n";
                return;
            }
        }
        else if(arg == "--vfs"){
            vfs = true;
        }
        else if(arg == "--no-remove"){
            no_remove = true;
        }
        else if (arg == "-i" || arg == "--image") {
            if (i + 1 < cmds.size()) {
                image_name = cmds[++i];

                i++;
                while(i < cmds.size()) {
                    commands.emplace_back(cmds[i++]);
                }
                break;
            } else {
                std::cerr << "Error: -i requires an image name\n";
                return;
            }
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            Utils::print_usage();
            return;
        }
        i++;
    }

    if (image_name.empty()) {
        std::cerr << "Error: Image name required (-i)\n";
        return;
    }

    CommandLineHandler::pull(db, {image_name});
    container_id = Utils::generate_container_id();
    Container container{ container_name, container_id.substr(0,6), root_fs, volumes, forward_ports, container_id, db, image_name, vfs, no_remove };
    container.exec("/bin/bash", commands);
}

void CommandLineHandler::attach(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() != 1){
        Utils::print_usage();
        return;
    }
    container_id = cmds[0];
    container_pid = { db_manager.get_container(container_id).pid };
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

    std::cout << "CONTAINER ID\tNAME\tIMAGE\tSTATUS\tCREATED AT\tVFS PATH\n";
    for (const auto& container : containers) {
        std::cout << container.id << "\t"
            << container.name << "\t"
            << container.image << "\t"
            << container.status << "\t"
            << container.created_at << "\t"
            << container.vfs_path << '\n';
    }
}

void CommandLineHandler::rm(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.empty()){
        Utils::print_usage();
        return;
    }
    for(const std::string& cmd : cmds){
        container_id = cmd;
        ContainerObject container_obj{ db_manager.get_container(container_id) };

        if(db_manager.container_exists(container_id)){
            if ((container_obj.status == "stopped" || container_obj.status == "exited") && db_manager.remove_container(container_id)) {
                db_manager.remove_volumes_by_id(container_id);
                db_manager.remove_networks_by_id(container_id);
                std::cout << "Container " << container_id << " removed.\n";
            }
            else {
                Utils::handle_error(container_id + " is running.");
            }
        }
        else{
            Utils::handle_error(container_id + " doesn't exist");
        }
    }
}

void CommandLineHandler::image(DatabaseManager& db_manager, ImageManager& img_manager, const std::vector<std::string>&  cmds){
    if (cmds.empty()) {
        Utils::print_usage();
        return;
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
        if (cmds.size() < 2) Utils::handle_error("Error: image_name:tag is required after rm");
        size_t no_cmds{ cmds.size() };
        for(size_t i{1};i<no_cmds;++i){
            image_name = cmds[i];
            size_t pos{ image_name.find(':') };
            std::string name{};
            std::string tag{};
            if(pos != std::string::npos){
                name = image_name.substr(0,pos);
                tag = image_name.substr(pos+1);
            }
            else{
                name = image_name;
                tag = "latest";
            }
            std::vector<ContainerObject> containers{ db_manager.list_containers_by_image(name, tag) };
            if(!containers.empty()){
                Utils::handle_error("Error: Image " + image_name + " is being used");
            }
            std::string err;
            if(db_manager.remove_image(name, tag)) {
                std::cout << image_name << " Removed from database\n";
            }
            else{
                std::cout << "Error: Cannot remove " << image_name << " from database\n";
                exit(EXIT_FAILURE);
            }
            if(img_manager.remove(image_name, err)) {
                std::cout << "Image " << image_name << " removed successfully from local storage.\n";
            } else {
                std::cout << "Failed to remove image " << image_name << " from local storage.\n";
                exit(EXIT_FAILURE);
            }
        }

    }
    else if(cmds[0] == "cls"){
        if(cmds.size() == 2){
            image_name = cmds[1];
            size_t pos{ image_name.find(':') };
            std::string name{};
            std::string tag{};
            if(pos != std::string::npos){
                name = image_name.substr(0,pos);
                tag = image_name.substr(pos+1);
            }
            else{
                name = image_name;
                tag = "latest";
            }
            std::vector<ContainerObject> containers{ db_manager.list_containers_by_image(name, tag) };

            std::cout << "CONTAINER ID\tNAME\tIMAGE\tPID\tSTATUS\tCREATED AT\tHOSTNAME\tFILESYSTEM PATH\tPTY SHELL\n";
            for (const auto& container : containers) {
                std::cout << container.id << "\t"
                    << container.name << "\t"
                    << container.image << "\t"
                    << container.status << "\t"
                    << container.created_at << "\t"
                    << container.hostname << "\t";
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
    if(cmds.empty()){
        Utils::print_usage();
        return;
    }
    if(cmds[0] == "ls"){
        std::vector<VolumeObject> volumes = db_manager.list_all_volumes();
        std::cout << "VOLUME ID\tCONTAINER ID\tHOST PATH\tCONTAINER PATH\n";
        for (const auto& volume : volumes) {
            std::cout << volume.id << "\t"
                << volume.container_id << "\t"
                << volume.host_path << "\t"
                << volume.container_path << "\n";
        }
    }
    else if(cmds[0] == "rm"){
        size_t i{ 1 };
        while(i < cmds.size()){
            int volume_id{ std::stoi(cmds[i]) };
            if(db_manager.volume_exists(volume_id)){
                if(db_manager.remove_volume(volume_id)){
                    std::cout << "Successfully removed " + cmds[i] + " volume\n";
                }
                else{
                    Utils::handle_error("Unable to remove " + std::to_string(volume_id));
                }
            }
            else{
                Utils::handle_error(std::to_string(volume_id) + " doesn't exist");
            }
            ++i;
        }
    }
    else{
        Utils::print_usage();
    }
}

void CommandLineHandler::network(DatabaseManager &db_manager, const std::vector<std::string> &cmds){
    if(cmds.empty()){
        Utils::print_usage();
        return;
    }
    if(cmds[0] == "ls"){
        std::vector<NetworkObject> networks{ db_manager.get_all_networks() };
        std::cout << "NETWORK ID\tCONTAINER ID\tHOST PORT\tCONTAINER PORT\n";
        for (const NetworkObject& network : networks) {
            std::cout << network.id << "\t"
                << network.container_id << "\t"
                << network.host_port << "\t"
                << network.container_port << "\n";
        }
    }
    else if(cmds[0] == "rm"){
        size_t i{ 1 };
        while(i < cmds.size()){
            int network_id{ std::stoi(cmds[i]) };
            if(db_manager.network_exists(network_id)){
                if(db_manager.remove_network(network_id)){
                    std::cout << "Successfully removed " + cmds[i] + " network\n";
                }
                else{
                    Utils::handle_error("Unable to remove " + std::to_string(network_id));
                }
            }
            else{
                Utils::handle_error(std::to_string(network_id) + " doesn't exist");
            }
            ++i;
        }
   }
    else{
        Utils::print_usage();
    }
}

void CommandLineHandler::create(DatabaseManager& db_manager,const std::vector<std::string>& cmds){
    if(cmds[0] == "volume"){
        size_t i{ 2 };
        while (i < cmds.size()) {
            container_id = cmds[1];
            if(!db_manager.container_exists(container_id)){
                Utils::handle_error(container_id + " doesn't exists");
            }
            std::string volume_input{ cmds[i] };
            size_t sep_pos { volume_input.find(':') };
            if (sep_pos == std::string::npos) {
                std::cerr << "Invalid volume format. Expected host_path:container_path\n";
                break;
            }
            std::string host_path{ volume_input.substr(0, sep_pos) };
            std::string container_path{ volume_input.substr(sep_pos + 1) };

            VolumeObject volume;
            volume.container_id = container_id;
            volume.host_path = host_path;
            volume.container_path = container_path;

            db_manager.add_volume(volume);
            ++i;
        }
    }
    else if(cmds[0] == "network"){
        size_t i{ 2 };
        while (i < cmds.size()) {
            container_id = cmds[1];
            if(!db_manager.container_exists(container_id)){
                Utils::handle_error(container_id + " doesn't exists");
            }
            std::string network_input{ cmds[i] };
            size_t sep_pos { network_input.find(':') };
            if (sep_pos == std::string::npos) {
                std::cerr << "Invalid network format. Expected host_port:container_port\n";
                break;
            }
            std::string host_port{ network_input.substr(0, sep_pos) };
            std::string container_port{ network_input.substr(sep_pos + 1) };

            NetworkObject network{};
            network.container_id = container_id;
            network.host_port = std::stoi(host_port);
            network.container_port = std::stoi(container_port);
            db_manager.add_ports(network);
            ++i;
        }
        if(i == 2) Utils::print_usage();
    }
}

void CommandLineHandler::pull(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() != 1){
        Utils::print_usage();
        return;
    }
    image_name = cmds[0];

    ImageManager img_manager(db_manager);
    std::string out_path{};
    std::string err{};
    if (!img_manager.pull(image_name, out_path, err)) {
        std::cout << "Failed to pull image: " << err << std::endl;
        exit(EXIT_FAILURE);
    } else {
        std::cout << "Image pulled successfully to: " << out_path << std::endl;
    }
    root_fs = out_path;
}

void CommandLineHandler::start(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() != 1){
        std::cout << "Error: Container id is required\n";
        Utils::print_usage();
        return;
    }
    container_id = cmds[0];

    ContainerObject container_obj{ db_manager.get_container(container_id) };
    if (container_obj.id.empty()) {
        std::cerr << "Error: Container " << container_id << " not found.\n";
        return;
    }

    std::string container_name{ container_obj.name };
    image_name = container_obj.image;

    std::vector<VolumeObject> vols{ db_manager.get_container_volumes(container_id) };

    forward_ports = db_manager.get_linked_ports(container_id);

    Container container{
        container_name,
        container_id.substr(0,6),
        container_obj.filesystem_path,
        vols,
        forward_ports,
        container_id,
        db_manager,
        image_name,
        container_obj.vfs,
        container_obj.no_remove
    };

    if (db_manager.update_container_status(container_id, "running")) {
        std::cout << "Starting container " << container_id << "...\n";
    } else {
        std::cout << "Failed to update status of " << container_id << " to RUNNING.\n";
        return;
    }

    container.exec("/bin/bash", commands);
}

void CommandLineHandler::stop(DatabaseManager& db_manager, const std::vector<std::string>& cmds){
    if(cmds.size() != 1){
        std::cout << "Error: Container id is required\n";
        Utils::print_usage();
        return;
    }
    container_id = cmds[0];

    ContainerObject obj{ db_manager.get_container(container_id) };
    if(obj.id.empty()) {
        std::cerr << "Error: Container not found.\n";
        return;
    }

    container_pid = obj.pid;
    pid_t net_pid{ obj.net_pid };

    Container container{};
    container.stop(container_pid, net_pid);

    if (db_manager.update_container_status(container_id, "stopped")) {
        std::cout << "Container " << container_id << " stopped successfully.\n";
    } else {
        std::cout << "Failed to update status of " << container_id << " in database.\n";
    }
}
