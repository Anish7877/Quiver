#include "../include/process.hpp"
#include "../include/tty_proxy_server.hpp"
#include "../include/image_management.hpp"

int main(int argc,char* argv[]) {
    ImageManager image{};
    std::string filesystem_path{};
    std::string err{};
    TTYProxyServer tty{};
    if(argc > 1 && strcmp(argv[1],"attach") != 0 && image.pull(argv[1],filesystem_path,err)){
        Process p{};
        if(p.start("container",filesystem_path,"/bin/bash") == -1){
            _exit(0);
        }
    }
    else if(argc > 1 && strcmp(argv[1],"attach") == 0){
        tty.reattach_to_socket(argv[2]);
    }
    return 0;
}

