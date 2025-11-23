#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>

struct ContainerObject {
    std::string id;
    std::string name;
    std::string image;
    pid_t pid;
    pid_t net_pid;
    std::string status;
    std::string created_at;
    std::string hostname;
    std::string filesystem_path;
    std::string pty_shell;
    bool vfs;
    bool no_remove;
    std::string vfs_path;
};

struct VolumeObject {
    int id;
    std::string container_id;
    std::string host_path;
    std::string container_path;
};

struct ImageObject {
    int id;
    std::string name;
    std::string tag;
    std::string path;
    long long size;
    std::string created_at;
};

struct NetworkObject{
    int id;
    std::string container_id;
    int host_port;
    int container_port;
};

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    bool init_db();
    void manual_cleanup();

    bool add_container(const ContainerObject& container);
    ContainerObject get_container(const std::string& container_id);
    bool update_container_status(const std::string& container_id, const std::string& status);
    bool update_container_pid(const std::string& container_id, pid_t pid);
    bool remove_container(const std::string& container_id);
    bool container_exists(const std::string& container_id);
    std::vector<ContainerObject> list_all_containers();
    std::vector<ContainerObject> list_running_containers();
    std::vector<ContainerObject> list_containers_by_image(const std::string& image_name, const std::string& tag);

    bool add_volume(const VolumeObject& volume);
    bool remove_volume(const int& volume_id);
    bool update_container_name_in_volumes(const int& volume_id, const std::string& container_id);
    bool volume_exists(const int& volume_id);
    std::vector<VolumeObject> list_all_volumes();
    std::vector<VolumeObject> get_container_volumes(const std::string& container_id);
    void remove_volumes_by_id(const std::string& container_id);

    std::vector<ImageObject> list_all_images();
    bool add_image(const std::string& image_name, const std::string& image_path, long long image_size);
    bool remove_image(const std::string& image_name, const std::string& tag);

    bool network_exists(const int& network_id);
    bool remove_network(const int& network_id);
    bool create_ports(const std::string& container_id, const std::vector<std::pair<int, int>>& ports);
    bool add_ports(const NetworkObject& network);
    std::vector<NetworkObject> get_all_networks();
    std::vector<std::pair<int,int>> get_linked_ports(const std::string& container_id);
    void remove_networks_by_id(const std::string& container_id);

private:
    sqlite3* m_db;
    std::string m_db_path;

};
