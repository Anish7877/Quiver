#pragma once
#include <vector>
#include <string>
#include <sys/mount.h>
#include <sys/stat.h>
#include "../include/database_manager.hpp"

class VolumeManager{
    public:
        enum class VolumeType{
            temporary,
            permanent
        };
        explicit VolumeManager() = default;
        explicit VolumeManager(const std::vector<std::string>& volumes, const std::string& rootfs, DatabaseManager& db, const std::string& container_id);
        ~VolumeManager(){};
        void create_volume(const std::vector<std::string>& volumes, const VolumeType& type = VolumeType::permanent);
        void remove_volume(const std::vector<std::string>& volumes);
        void link_volume(const std::vector<std::string>& volumes, const std::vector<std::string>& containers);
        void unlink_volume(const std::vector<std::string>& volumes, const std::vector<std::string>& containers);
    private:
        std::vector<std::string> m_volumes{};
        std::string m_rootfs{};

        DatabaseManager& m_db;
        std::string m_container_id{};
};
