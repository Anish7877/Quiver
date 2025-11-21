#pragma once
#include <sys/stat.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <string>

class Network {
    public:
        Network() = default;

        static int setup_networking(const pid_t& pid);
        static int setup_networking_with_ports(const pid_t& pid, const std::vector<std::pair<int, int>>& port_forwards);

        static int forward_port(const int& host_port, const int& container_port);
        static int add_port_forward(const pid_t& container_pid, int host_port, int container_port);
        static int remove_port_forward(const pid_t& container_pid, int host_port);
        static int connect_namespaces(const pid_t& target_pid, int host_port, int target_port);

        static pid_t get_net_pid() { return m_net_pid; }
        static std::string get_container_ip(const pid_t& container_pid);
        static std::string get_api_socket_path(const pid_t& container_pid);
        static bool is_running();

        static bool ping_container(const pid_t& container_pid);
        static int send_api_command(const pid_t& container_pid, const std::string& json_command);

        static int cleanup_networking(const pid_t& container_pid);
        ~Network();

    private:
        struct NetworkConfig {
            pid_t slirp_pid;
            std::vector<std::pair<int, int>> port_forwards;
            std::string api_socket;
        };

        static pid_t m_net_pid;
        static bool m_running;
        static std::map<pid_t, NetworkConfig> m_container_networks;
        static int wait_for_api_socket(const std::string& socket_path, int timeout_ms = 5000);
};
