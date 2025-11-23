#include "../include/volumes.hpp"
#include "../include/utils.hpp"
#include <iostream>

VolumeManager::VolumeManager(const std::vector<std::string>& volumes,
                             const std::string& rootfs,
                             DatabaseManager& db,
                             const std::string& container_id)
    : m_volumes{ volumes }
    , m_rootfs{ rootfs }
    , m_db{ db }
    , m_container_id{ container_id }
{}

void VolumeManager::create_volume(const std::vector<std::string>& volumes, const VolumeType& type){
    bool db_write{ false };
    if(type == VolumeType::temporary){
        db_write = false;
    }
    else if(type == VolumeType::permanent){
        db_write = true;
    }
    else{
        Utils::handle_error("Wrong Volume Type");
    }
    if(db_write){
        for(const std::string& volume_spec : volumes){
            size_t pos{ volume_spec.find(':') };
            if (pos == std::string::npos) {
                std::cerr << "Warning: Volume spec '" << volume_spec << "' is malformed, skipping DB entry." << '\n';
                continue;
            }

            VolumeObject v;
            v.container_id = m_container_id;
            v.host_path = volume_spec.substr(0, pos);
            v.container_path = volume_spec.substr(pos + 1);

            if (!m_db.add_volume(v)) {
                std::cerr << "Warning: Failed to add volume to database: " << volume_spec << '\n';
            }
        }
    }
}

void VolumeManager::remove_volume(const std::vector<std::string>& volumes){
    for(const std::string& volume_path : volumes){
        std::vector<VolumeObject> existing_volumes = m_db.get_container_volumes(m_container_id);

        for (const auto& v : existing_volumes) {
            if (v.container_path == volume_path || v.host_path == volume_path) {
                if (m_db.remove_volume(v.id)) {
                    std::cout << "Removed volume from DB: ID " << v.id << ", Path: " << v.container_path << '\n';
                }
            }
        }
    }
}

void VolumeManager::link_volume(const std::vector<std::string>& volumes,const std::vector<std::string>& containers){
    if(volumes.size() != containers.size()){
        Utils::handle_error("More containers or volumes provided");
    }
    size_t no{ volumes.size() };
    for(size_t i{0};i<no;++i){
        size_t pos{ volumes[i].find(':') };
        if (pos == std::string::npos) {
            std::cerr << "Warning: Volume spec '" << volumes[i] << "' is malformed, skipping DB link." << '\n';
            continue;
        }

        VolumeObject v;
        v.container_id = containers[i];
        v.host_path = volumes[i].substr(0, pos);
        v.container_path = volumes[i].substr(pos + 1);

        if (!m_db.add_volume(v)) {
            std::cerr << "Warning: Failed to link volume to container " << containers[i] << '\n';
        }
    }
}

void VolumeManager::unlink_volume(const std::vector<std::string>& volumes,const std::vector<std::string>& containers){
    if(volumes.size() != containers.size()){
        Utils::handle_error("More containers or volumes provided");
    }
    size_t no{ volumes.size() };
    for(size_t i{0};i<no;++i){
        std::vector<VolumeObject> existing_volumes = m_db.get_container_volumes(containers[i]);
        for (const auto& v : existing_volumes) {
            if (v.container_path == volumes[i] || v.host_path == volumes[i]) {
                if (m_db.remove_volume(v.id)) {
                    std::cout << "Unlinked volume from DB: ID " << v.id << ", Container: " << containers[i] << '\n';
                }
            }
        }
    }
}
