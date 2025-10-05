#include "../include/device_manager.hpp"

void DeviceManager::create_terminal_devices(){
    // mount /dev/pts to /dev of merged rootfs
    // then mknod or bind mount from user /dev/{null,zero,random,urandom,tty,pts,ptmx}
    // symlink /dev/tty -> /dev/pts/0
    // symlink stdin -> /dev/stdin
    // symlink stdout -> /dev/stdout
    // symlink stderr -> /dev/stderr
}
