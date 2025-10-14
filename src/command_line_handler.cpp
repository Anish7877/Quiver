#include "../include/command_line_handler.hpp"
#include "../include/utils.hpp"
#include <vector>

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
    // write to database after parsing
}

void CommandLineHandler::attach(const std::vector<std::string>& cmds){
    if(cmds.size() > 2){
        Utils::print_usage();
    }
    container_pid = std::stoi(cmds[1]);
}

void CommandLineHandler::ps(const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    if(cmds.size() == 0){
        // To do list all running container table of database in table form
    }
    else if(cmds.size() == 1 && cmds[0] == "-a"){
        // To do list all the containers in database
    }
}

void CommandLineHandler::rm(const std::vector<std::string>& cmds){
    if(cmds.size() > 2){
        Utils::print_usage();
    }
    container_name = cmds[1];
    // remove db entry from database of container
}

void CommandLineHandler::image(const std::vector<std::string>& cmds){
    if(cmds[0] == "ls"){
        // list all the images downloaded and their versions
    }
    else if(cmds[0] == "rm"){
        if(cmds.size() > 1){
            image_name = cmds[1];
            // remove image from the local storage
        }
        else{
            Utils::print_usage();
        }
    }
    else if(cmds[0] == "cls"){
        if(cmds.size() > 1){
            image_name = cmds[1];
            // list containers which are using a particular image
        }
        else{
            Utils::print_usage();
        }
    }
}

void CommandLineHandler::volume(const std::vector<std::string>& cmds){
    if(cmds[0] == "ls"){
        // list all the volumes database in tabular form
    }
    // baaki abhi not sure
}

void CommandLineHandler::create(const std::vector<std::string>& cmds){
    if(cmds[0] == "container"){
        std::vector<std::string> new_cmds{ cmds };
        new_cmds.erase(new_cmds.begin());
        run(new_cmds);
    }
    else if(cmds[0] == "volume"){
        // write to database for further volume details
    }
}

void CommandLineHandler::pull(const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    image_name = cmds[0];
    // pull image and save to local storage
}

void CommandLineHandler::start(const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    container_name = cmds[0];
    // start the stopped container with loaded info
}

void CommandLineHandler::stop(const std::vector<std::string>& cmds){
    if(cmds.size() > 1){
        Utils::print_usage();
    }
    // stop the container shell and make running status false in database
}
