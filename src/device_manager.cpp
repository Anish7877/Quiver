#include "../include/device_manager.hpp"
#include "../include/utils.hpp"
#include "../include/mount.hpp"
#include <unistd.h>

void DeviceManager::create_terminal_devices(const std::string& rootfs, const std::string& dev_pts_path){
    // mount /dev/pts to /dev of merged rootfs
    // then mknod or bind mount from user /dev/{null,zero,random,urandom,tty,pts,ptmx}
    // symlink /dev/tty -> /dev/pts/0
    // symlink stdin -> /dev/stdin
    // symlink stdout -> /dev/stdout
    // symlink stderr -> /dev/stderr
    int flags{ MS_NOSUID | MS_NOEXEC };
    std::string options{ "newinstance,ptmxmode=0666,mode=620,gid=5" };
    Utils::ensure_dirs(dev_pts_path);
    Mount::dev_pts(dev_pts_path, flags, options);

    std::string dev_base{ "/dev/" };
    std::vector<std::string> devices{ "null", "zero", "random", "urandom", "tty", "pts", "ptmx" };
    for(const std::string& device : devices){
        std::string src{ dev_base + device };
        std::string dst{ rootfs + dev_base + device };
        Utils::ensure_dirs(src);
        Utils::ensure_dirs(dst);
        Mount::bind_mount(src, dst);
    }

    std::string ptmx_link{ rootfs + dev_base + "ptmx" };
    if(symlink("pts/ptmx", ptmx_link.c_str()) == -1)
        Utils::handle_error("Unable to create symlink from pts/ptmx to " + ptmx_link);
}
