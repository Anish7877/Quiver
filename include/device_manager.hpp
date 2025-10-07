#pragma once
#include <string>

namespace DeviceManager{
    void create_terminal_devices(const std::string& rootfs, const std::string& dev_pts_path);
}
