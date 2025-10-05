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

        // Basic networking setup
        static int setup_networking(const pid_t& pid);

        // Enhanced networking with port forwarding
        static int setup_networking_with_ports(const pid_t& pid,
                                             const std::vector<std::pair<int, int>>& port_forwards);

        // Port forwarding management
        static int forward_port(const int& host_port, const int& container_port);
        static int add_port_forward(const pid_t& container_pid, int host_port, int container_port);
        static int remove_port_forward(const pid_t& container_pid, int host_port);

        // Network information
        static pid_t get_net_pid() { return m_net_pid; }
        static std::string get_container_ip(const pid_t& container_pid);
        static std::string get_api_socket_path(const pid_t& container_pid);

        // Communication helpers
        static bool ping_container(const pid_t& container_pid);
        static int send_api_command(const pid_t& container_pid, const std::string& json_command);

        // Cleanup
        static int cleanup_networking(const pid_t& container_pid);
        ~Network();

    private:
        struct NetworkConfig {
            pid_t slirp_pid;
            std::vector<std::pair<int, int>> port_forwards;
            std::string api_socket;
        };

        static pid_t m_net_pid;
        static std::map<pid_t, NetworkConfig> container_networks;

        // Helper functions
        static int wait_for_api_socket(const std::string& socket_path, int timeout_ms = 5000);
        static int create_port_forward_args(const std::vector<std::pair<int, int>>& forwards,
                                          std::vector<std::string>& args);
};
