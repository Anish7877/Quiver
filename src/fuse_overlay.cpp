#include "../include/fuse_overlay.hpp"
#include "../include/utils.hpp"
#include <sys/wait.h>

void FuseOverlay::setup(const std::string& lower, const std::string& upper, const std::string& work, const std::string& merged){
    pid_t pid{ fork() };
    if(pid == 0){
        std::string fuse_path{ "/usr/bin/fuse-overlayfs" };
        if(execlp(fuse_path.c_str(), fuse_path.c_str(),
                "-o", ("lowerdir=" + lower).c_str(),
                "-o", ("upperdir=" + upper).c_str(),
                "-o", ("workdir=" + work).c_str(),
                merged.c_str(),
                (char*) NULL) == -1){
            Utils::handle_error("Overlay filesystem error");
        }
        exit(1);
    }
    else{
        waitpid(pid, nullptr, 0);
    }
}
