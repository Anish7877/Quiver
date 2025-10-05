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
        static bool path_exist(const std::string& path);
        static Managers get_manager();
};
