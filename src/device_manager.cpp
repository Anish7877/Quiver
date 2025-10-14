#include "../include/device_manager.hpp"
#include "../include/utils.hpp"
#include "../include/mount.hpp"
#include <sys/stat.h>
#include <cstring>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <iostream>

void DeviceManager::create_terminal_devices(){
    struct DeviceBind {
        const char* name;
        bool required;
    };

    DeviceBind devices[]{
        {"null", true},
        {"zero", true},
        {"full", true},
        {"random", true},
        {"urandom", true},
        {"tty", true},
        {"console", false},
    };

    for (const auto& device : devices) {
        std::string old_path{ std::string("/old_root/dev/") + device.name };
        std::string new_path{ std::string("/dev/") + device.name };
        struct stat st{};
        if (stat(old_path.c_str(), &st) != 0) {
            if (device.required) {
                std::cerr << "WARNING: " << old_path << " not found in old root" << '\n';
            }
            continue;
        }

        int fd{ open(new_path.c_str(), O_CREAT | O_RDONLY, 0666) };
        if (fd >= 0) close(fd);
        Mount::bind_mount(old_path, new_path);
    }

    std::cerr << "DEBUG: Setting up /dev/pts..." << '\n';
    Utils::ensure_dirs("/dev/pts");

    int devpts_flags{ MS_NOSUID | MS_NOEXEC };
    std::string devpts_options{ "newinstance,ptmxmode=0666" };
    std::string devpts_second_opt{ "ptmxmode=0666" };
    Mount::dev_pts("/dev/pts", devpts_flags, devpts_options);
    Mount::dev_pts("/dev/pts", devpts_flags, devpts_second_opt);

    unlink("/dev/ptmx");
    if (symlink("pts/ptmx", "/dev/ptmx") == ERR) {
        std::cerr << "DEBUG: symlink /dev/ptmx failed, trying bind mount: " << strerror(errno) << '\n';
        // Try bind mounting from old root
        int fd{ open("/dev/ptmx", O_CREAT | O_RDONLY, 0666) };
        if (fd >= 0) close(fd);
        Mount::bind_mount("/old_root/dev/ptmx", "/dev/ptmx");
    }

    symlink("/proc/self/fd", "/dev/fd");
    symlink("/proc/self/fd/0", "/dev/stdin");
    symlink("/proc/self/fd/1", "/dev/stdout");
    symlink("/proc/self/fd/2", "/dev/stderr");

    Utils::ensure_dirs("/dev/shm");
    if (mount("shm", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777") == ERR) {
        std::cerr << "WARNING: mount /dev/shm failed: " << strerror(errno) << '\n';
    }
    std::cerr << "DEBUG: Device setup complete" << '\n';
}
