#include "../include/mount.hpp"
#include "../include/utils.hpp"
#include <sys/mount.h>

void Mount::proc(const std::string& proc_path, const int& flags){
    if(mount(PROC, proc_path.c_str(), PROC, flags, nullptr) == ERR)
        Utils::handle_error("Unable to mount " + std::string(PROC));
}

void Mount::dev(const std::string& dev_path, const int& flags){
    if(mount(DEV, dev_path.c_str(), DEV, flags, nullptr) == ERR)
        Utils::handle_error("Unable to mount " + std::string(DEV));
}

void Mount::sys(const std::string& sys_path, const int& flags){
    if(mount(SYS, sys_path.c_str(), SYS, flags, nullptr) == ERR)
        Utils::handle_error("Unable to mount " + std::string(SYS));
}

void Mount::tmpfs(const std::string& tmpfs_path, const int& flags){
    if(mount(TMPFS, tmpfs_path.c_str(), TMPFS, flags, nullptr) == ERR)
        Utils::handle_error("Unable to mount " + std::string(PROC));
}

void Mount::dev_pts(const std::string& dev_pts_path, const int& flags){
    if(mount(DEVPTS, dev_pts_path.c_str(), DEVPTS, flags, nullptr) == ERR)
        Utils::handle_error("Unable to mount " + std::string(DEVPTS));
}

void Mount::volumes(const std::string& rootfs, const std::vector<std::string> volumes){
    size_t no_volumes{ volumes.size() };
    if(no_volumes > 0){
        std::vector<std::string> paths{};
        std::vector<std::string> dirs{};
        for(size_t i{0};i<no_volumes;++i){
            size_t pos{ volumes[i].find(':') };
            paths.emplace_back(volumes[i].substr(0,pos));
            dirs.emplace_back(volumes[i].substr(pos+1));
        }
        for(size_t i{0};i<dirs.size();++i){
            dirs[i] = rootfs + dirs[i];
            Utils::ensure_dirs(dirs[i]);
        }
        for(size_t i{0};i<paths.size();++i){
            if(mount(paths[i].c_str(), dirs[i].c_str(), nullptr, MS_BIND|MS_REC, nullptr) == -1){
                Utils::handle_error("Unable to mount " + paths[i] + " to " + dirs[i]);
            }
        }
    }
}
