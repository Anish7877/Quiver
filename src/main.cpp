#include "../include/image_management.hpp"
#include "../include/container.hpp"
#include <string>

int main(int argc,char* argv[]) {
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

