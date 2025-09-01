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
