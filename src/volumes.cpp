#include "../include/volumes.hpp"
#include "../include/utils.hpp"
#include "../include/mount.hpp"

// TASK : create a new database only for volumes
// 1. i have marked with comments where database entries and removals are done
// 2. attributes in database are : container_name or container id
//                                 volumes are like (host path):(container path)
VolumeManager::VolumeManager(const std::vector<std::string>& volumes, const std::string& rootfs)
    : m_volumes{ volumes }
    , m_rootfs{ rootfs }
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
    }
    Mount::volumes(m_rootfs, m_volumes);
}

void VolumeManager::remove_volume(const std::vector<std::string>& volumes){
    for(const std::string& vollume : volumes){
        // remove database entries and their links to a container
    }
}

void VolumeManager::link_volume(const std::vector<std::string>& volumes,const std::vector<std::string>& containers){
    if(volumes.size() != containers.size()){
        Utils::handle_error("More containers or volumes provided");
    }
    size_t no{ volumes.size() };
    for(size_t i{0};i<no;++i){
        // add db entries for all volumes and links with them
    }
}

void VolumeManager::unlink_volume(const std::vector<std::string>& volumes,const std::vector<std::string>& containers){
    if(volumes.size() != containers.size()){
        Utils::handle_error("More containers or volumes provided");
    }
    size_t no{ volumes.size() };
    for(size_t i{0};i<no;++i){
        // remove db entries for all volumes and links with them
    }
}
