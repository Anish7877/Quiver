#pragma once
#include <string>

namespace DeviceManager{
    struct DeviceBind {
        const char* name;
        bool required;
    };
    void create_terminal_devices();
}
