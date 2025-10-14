#include "../include/fuse_overlay.hpp"
#include "../include/utils.hpp"
#include <sys/wait.h>
#include "../include/fuse_overlay.hpp"
#include "../include/utils.hpp"
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <fstream>

bool is_mountpoint(const std::string& path) {
    std::ifstream mounts("/proc/mounts");
    std::string line;
    while(std::getline(mounts, line)) {
        if(line.find(path) != std::string::npos &&
           line.find("fuse.fuse-overlayfs") != std::string::npos) {
            return true;
        }
    }
    return false;
}

void FuseOverlay::setup(const std::string& lower, const std::string& upper, const std::string& work, const std::string& merged){

    if(is_mountpoint(merged)) {
        std::cerr << "FUSE overlay already mounted at " << merged << '\n';
        return;
    }

    pid_t pid{ fork() };
    if(pid == 0){
        std::string fuse_path{ "/usr/bin/fuse-overlayfs" };

        // Let fuse-overlayfs daemonize naturally (no -f flag)
        if(execlp(fuse_path.c_str(), fuse_path.c_str(),
                "-o", ("lowerdir=" + lower).c_str(),
                "-o", ("upperdir=" + upper).c_str(),
                "-o", ("workdir=" + work).c_str(),
                merged.c_str(),
                (char*) NULL) == ERR){
            Utils::handle_error("Overlay filesystem error: exec failed");
        }
        exit(1);
    }
    else if(pid > 0){
        int status;
        waitpid(pid, &status, 0);

        if(WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            Utils::handle_error("fuse-overlayfs exited with error");
        }

        int max_attempts{ 10 };
        bool mounted{ false };

        for(int i = 0; i < max_attempts; i++){
            if(is_mountpoint(merged)){
                mounted = true;
                std::cerr << "FUSE overlay mounted successfully at " << merged << '\n';
                break;
            }
            usleep(100000);
        }

        if(!mounted){
            Utils::handle_error("FUSE overlay mount timeout - check /proc/mounts");
        }
    }
    else{
        Utils::handle_error("Fork failed for fuse-overlayfs");
    }
}
