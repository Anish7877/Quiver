#include "../include/volumes.hpp"
#include "../include/utils.hpp"
#include "../include/mount.hpp"
#include <iostream>

// TASK : create a new database only for volumes
// 1. i have marked with comments where database entries and removals are done
// 2. attributes in database are : container_name or container id
//                                 volumes are like (host path):(container path)
VolumeManager::VolumeManager(const std::vector<std::string>& volumes, const std::string& rootfs, DatabaseManager& db, const std::string& container_id)
    : m_volumes{ volumes }
    , m_rootfs{ rootfs }
    , m_db{ db } // Initialize the reference
    , m_container_id{ container_id } // Initialize the container ID
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
        // TO DO : write to the volumes database
        // along with merged rootfs
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
    Mount::volumes(m_rootfs, m_volumes);
}

void VolumeManager::remove_volume(const std::vector<std::string>& volumes){
    // Assuming 'volumes' here contains container_path or host_path strings of volumes to remove
    for(const std::string& volume_path : volumes){
        // 1. Get all volumes for this container
        std::vector<VolumeObject> existing_volumes = m_db.get_container_volumes(m_container_id);

        // 2. Find and remove matching volumes
        for (const auto& v : existing_volumes) {
            // Check if the input path matches the container path or host path
            if (v.container_path == volume_path || v.host_path == volume_path) {
                // remove database entries and their links to a container
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
        // add db entries for all volumes and links with them
        size_t pos{ volumes[i].find(':') };
        if (pos == std::string::npos) { 
            std::cerr << "Warning: Volume spec '" << volumes[i] << "' is malformed, skipping DB link." << '\n';
            continue; 
        }

        VolumeObject v;
        v.container_id = containers[i]; // The target container ID
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
        // remove db entries for all volumes and links with them
        
        // Find the volume record(s) to remove (volumes[i] is container_path or host_path, containers[i] is the container ID)
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
