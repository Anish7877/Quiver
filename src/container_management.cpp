//#include "../include/container_management.hpp"
//#include <chrono>
//#include <cstdint>
//#include <random>
//#include <string>
//
//// container args implementation
//long long ContainerArgs::m_ids = 1;
//ContainerArgs::ContainerArgs()
//    : m_container_id{ "None" }
//    , m_container_name{ "container " + std::to_string(m_ids) }
//    , m_host_name{ m_container_id }
//    , m_network{ "None" }
//    , m_filesystem_path { "None" }
//    , m_volume_attached { "None" }
//    , m_pty_shell { "None" }
//    , m_host_pid{ getpid() }
//    , m_running{ true }
//    , m_attached{ false }
//{ ++m_ids; }
//ContainerArgs::ContainerArgs(std::string_view host_name,std::string_view container_name)
//    : m_container_id{ "None" }
//    , m_container_name{ container_name == "container 1" || container_name.empty() ? "container " + std::to_string(m_ids) : container_name}
//    , m_host_name{ host_name }
//    , m_network{ "None" }
//    , m_filesystem_path { "None" }
//    , m_volume_attached { "None" }
//    , m_pty_shell { "None" }
//    , m_host_pid{ getpid() }
//    , m_running{ true }
//    , m_attached{ false }
//{ ++m_ids; }
//
//
//// conatiners management main implementation
//Containers::Containers(){
//    create_necessary_directories();
//    create_necessary_files();
//}
//void Containers::create_necessary_directories(){
//    for(int i{0};i<static_cast<int>(/*something*/);++i){
//    }
//}
//void Containers::create_necessary_files(){
//    for(int i{0};i<static_cast<int>(/*something*/);++i){
//    }
//}
//std::string Containers::get_username(){
//}
//std::string Containers::container_id_generator(const std::string& seed){
//    unsigned char hash[SHA256_DIGEST_LENGTH];
//    SHA256(reinterpret_cast<const unsigned char*>(seed.c_str()), seed.size(), hash);
//    std::ostringstream oss{};
//    for(int i{0};i<SHA256_DIGEST_LENGTH;++i){
//        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
//    }
//    return oss.str();
//}
//std::string Containers::seed_generator(){
//    auto now{ std::chrono::high_resolution_clock::now().time_since_epoch().count() };
//    std::random_device rd{};
//    std::mt19937 mt{ rd() };
//    std::uniform_int_distribution<uint64_t> dist;
//    uint64_t random_num { dist(mt) };
//    return std::to_string(now) + "_" + std::to_string(random_num);
//}
//

#include "../include/container_management.hpp"
#include "../include/utils.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <iostream>

// Constructor that takes a reference to the DatabaseManager
ContainerManager::ContainerManager(DatabaseManager& db) : m_db(db) {}

// Creates a new container and saves it to the database
std::string ContainerManager::create_container(const std::string& image_name, const std::string& container_name, const std::string& hostname) {
    // 1. Generate a unique ID for the container
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::string seed = image_name + container_name + std::to_string(now);
    std::string container_id = generate_container_id(seed);

    // 2. Create a Container struct with the initial data
    Container c;
    c.id = container_id;
    c.name = container_name.empty() ? "quiver-" + container_id.substr(0, 12) : container_name;
    c.image = image_name;
    c.pid = 0;
    c.status = "created";
    c.hostname = hostname;
    c.filesystem_path = Utils::get_filesystem_path(getpid()); // TODO: Placeholder, may need adjustment
    c.pty_shell = "/bin/sh";

    // 3. Add the container to the database
    if (!m_db.add_container(c)) {
        return "";
    }

    return container_id;
}

// Retrieves full container information from the database
Container ContainerManager::get_container_info(const std::string& container_id) {
    return m_db.get_container(container_id);
}

// Lists all containers stored in the database
std::vector<Container> ContainerManager::list_all_containers() {
    return m_db.list_containers();
}

// Removes a container and its associated data
bool ContainerManager::remove_container(const std::string& container_id) {
    // TODO: Also remove associated volumes here
    return m_db.remove_container(container_id);
}

// Logs container data to the console
void ContainerManager::log_container_data(const std::string& container_id) {
    Container c = get_container_info(container_id);
    if (!c.id.empty()) {
        std::cout << "--- Container Info ---" << std::endl;
        std::cout << "ID: " << c.id << std::endl;
        std::cout << "Name: " << c.name << std::endl;
        std::cout << "Image: " << c.image << std::endl;
        std::cout << "PID: " << c.pid << std::endl;
        std::cout << "Status: " << c.status << std::endl;
        std::cout << "Created: " << c.created_at << std::endl;
        std::cout << "----------------------" << std::endl;
    } else {
        std::cerr << "Could not find container with ID: " << container_id << std::endl;
    }
}


// Generates a unique SHA256 ID for a new container
std::string ContainerManager::generate_container_id(const std::string& seed) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, seed.c_str(), seed.size());
    SHA256_Final(hash, &sha256);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}