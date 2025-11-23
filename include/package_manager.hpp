#pragma once
#include <string>
#include <sys/stat.h>

class PackageManager{
    public:
        explicit PackageManager() = default;
        static int initialize();
        ~PackageManager(){}
    private:
        enum class Managers{
            apt,
            rcf,
            pacman,
            apk,
            zypper,
            unknown
        };
        static Managers get_manager();
};
