#include "../include/device_manager.hpp"
#include "../include/utils.hpp"
#include "../include/mount.hpp"
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <iostream>

void DeviceManager::create_terminal_devices(const std::string& rootfs, const std::string& dev_pts_path){
    std::cerr << "DEBUG: Creating terminal devices..." << '\n';
    std::cerr << "DEBUG: rootfs: '" << rootfs << "'" << '\n';
    std::cerr << "DEBUG: dev_pts_path: " << dev_pts_path << '\n';

    // Construct proper paths
    std::string dev_base = rootfs.empty() ? "/dev/" : rootfs + "/dev/";
    std::string pts_path = rootfs.empty() ? "/dev/pts" : rootfs + "/dev/pts";
    std::string shm_path = rootfs.empty() ? "/dev/shm" : rootfs + "/dev/shm";
    std::string ptmx_path = rootfs.empty() ? "/dev/ptmx" : rootfs + "/dev/ptmx";

    std::cerr << "DEBUG: Creating /dev/pts at: " << pts_path << '\n';

    // Create /dev/pts directory
    if (mkdir(pts_path.c_str(), 0755) == -1 && errno != EEXIST) {
        std::cerr << "ERROR: mkdir /dev/pts failed, errno=" << errno
                  << " (" << strerror(errno) << ")" << '\n';
        Utils::handle_error("Unable to create /dev/pts directory");
    }

    // Create /dev/shm directory
    std::cerr << "DEBUG: Creating /dev/shm at: " << shm_path << '\n';
    if (mkdir(shm_path.c_str(), 0755) == -1 && errno != EEXIST) {
        std::cerr << "WARNING: mkdir /dev/shm failed: " << strerror(errno) << '\n';
    }

    // Mount devpts
    int flags = MS_NOSUID | MS_NOEXEC;
    std::string options = "newinstance,ptmxmode=0666,mode=620,gid=5";

    std::cerr << "DEBUG: Mounting devpts..." << '\n';
    if (mount("devpts", pts_path.c_str(), "devpts", flags, options.c_str()) == -1) {
        std::cerr << "ERROR: devpts mount failed, errno=" << errno
                  << " (" << strerror(errno) << ")" << '\n';

        // Try with simpler options
        std::cerr << "DEBUG: Trying devpts with simpler options..." << '\n';
        options = "newinstance,ptmxmode=0666";
        if (mount("devpts", pts_path.c_str(), "devpts", flags, options.c_str()) == -1) {
            std::cerr << "ERROR: devpts mount still failed: " << strerror(errno) << '\n';
            Utils::handle_error("Unable to mount devpts");
        }
    }
    std::cerr << "DEBUG: devpts mounted successfully" << '\n';

    // Create symlink /dev/ptmx -> pts/ptmx
    std::cerr << "DEBUG: Creating ptmx symlink at: " << ptmx_path << '\n';

    // Remove if it already exists
    unlink(ptmx_path.c_str());

    if (symlink("pts/ptmx", ptmx_path.c_str()) == -1) {
        std::cerr << "ERROR: symlink pts/ptmx failed, errno=" << errno
                  << " (" << strerror(errno) << ")" << '\n';

        // If symlink fails, try to create ptmx as a character device
        std::cerr << "DEBUG: Trying to create /dev/ptmx as device node..." << '\n';
        if (mknod(ptmx_path.c_str(), S_IFCHR | 0666, makedev(5, 2)) == -1 && errno != EEXIST) {
            std::cerr << "WARNING: mknod /dev/ptmx failed: " << strerror(errno) << '\n';
        }
    }

    // Bind mount essential devices from host
    std::vector<std::string> devices{ "null", "zero", "random", "urandom", "tty", "full"};

    for(const std::string& device : devices){
        std::string src = "/dev/" + device;
        std::string dst = dev_base + device;

        std::cerr << "DEBUG: Bind mounting " << src << " to " << dst << '\n';

        // Verify source exists
        struct stat st;
        if (stat(src.c_str(), &st) != 0) {
            std::cerr << "WARNING: Source device " << src << " does not exist, skipping" << '\n';
            continue;
        }

        // Create destination if needed - as a regular file first, then bind mount over it
        int fd = open(dst.c_str(), O_CREAT | O_RDONLY, 0666);
        if (fd != -1) {
            close(fd);
        }

        // Bind mount the device
        if (mount(src.c_str(), dst.c_str(), nullptr, MS_BIND, nullptr) == -1) {
            std::cerr << "WARNING: bind mount " << src << " to " << dst
                      << " failed: " << strerror(errno) << '\n';
        }
    }

    std::cerr << "DEBUG: Terminal devices setup complete" << '\n';
}
