#include "../include/network.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <algorithm>

// Static member definitions
pid_t Network::m_net_pid{-1};
std::map<pid_t, Network::NetworkConfig> Network::container_networks;

int Network::setup_networking(const pid_t& pid){
    std::cout << "Setting Network for PID " << pid << '\n';
    m_net_pid = fork();
    if(m_net_pid == 0){
        // Detach from parent
        if(setsid() == -1){
            std::cerr << "SetSID failed" << '\n';
            _exit(1);
        }

        // Redirect output to avoid blocking
        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > 2) close(devnull);
        }

        std::string pid_str{std::to_string(pid)};
        execl("/usr/bin/slirp4netns", "slirp4netns",
              "--configure",           // Auto-configure network interface
              "--mtu=65520",          // Set MTU
              "--enable-sandbox",     // Your existing flag
              pid_str.c_str(),
              "tap0",
              (char*)NULL);

        std::cerr << "exec slirp4netns failed" << '\n';
        _exit(1);
    }
    else if(m_net_pid > 0){
        // Give slirp4netns time to start up
        usleep(500000); // 500ms
        return 0;
    }
    else{
        std::cerr << "failed to fork slirp4netns process" << '\n';
        return -1;
    }
}

int Network::setup_networking_with_ports(const pid_t& pid,
                                        const std::vector<std::pair<int, int>>& port_forwards) {
    std::cout << "Setting up network for PID " << pid << " with port forwarding\n";

    // Create API socket path
    std::string api_socket = "/tmp/slirp4netns-" + std::to_string(pid) + ".sock";

    pid_t slirp_pid = fork();
    if (slirp_pid == 0) {
        // Detach from parent
        if(setsid() == -1){
            std::cerr << "SetSID failed" << '\n';
            _exit(1);
        }

        // Redirect output
        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > 2) close(devnull);
        }

        std::vector<std::string> args;
        args.push_back("slirp4netns");
        args.push_back("--configure");
        args.push_back("--mtu=65520");
        args.push_back("--enable-sandbox");
        args.push_back("--api-socket");
        args.push_back(api_socket);

        // Add port forwards
        create_port_forward_args(port_forwards, args);

        args.push_back(std::to_string(pid));
        args.push_back("tap0");

        // Convert to char* array
        std::vector<char*> argv;
        for (auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execv("/usr/bin/slirp4netns", argv.data());
        _exit(1);
    }
    else if (slirp_pid > 0) {
        NetworkConfig config;
        config.slirp_pid = slirp_pid;
        config.port_forwards = port_forwards;
        config.api_socket = api_socket;

        container_networks[pid] = config;
        m_net_pid = slirp_pid;  // Keep for compatibility

        // Wait for API socket to be created
        if (wait_for_api_socket(api_socket) == 0) {
            std::cout << "Network setup complete for container " << pid << std::endl;
            return 0;
        } else {
            std::cerr << "Timeout waiting for network API" << std::endl;
            return -1;
        }
    }
    else {
        std::cerr << "Failed to fork slirp4netns process" << std::endl;
        return -1;
    }
}

int Network::forward_port(const int& host_port, const int& container_port) {
    // Legacy function - use the first container if available
    if (container_networks.empty()) {
        std::cerr << "No containers with networking configured" << std::endl;
        return -1;
    }

    pid_t first_container = container_networks.begin()->first;
    return add_port_forward(first_container, host_port, container_port);
}

int Network::add_port_forward(const pid_t& container_pid, int host_port, int container_port) {
    auto it = container_networks.find(container_pid);
    if (it == container_networks.end()) {
        std::cerr << "Container network not found for PID " << container_pid << std::endl;
        return -1;
    }

    std::string api_socket = it->second.api_socket;

    // Create JSON command for slirp4netns API
    std::string json_cmd = "{\"execute\": \"add_hostfwd\", \"arguments\": {\"proto\": \"tcp\", \"host_addr\": \"0.0.0.0\", \"host_port\": "
                          + std::to_string(host_port) + ", \"guest_addr\": \"10.0.2.100\", \"guest_port\": "
                          + std::to_string(container_port) + "}}";

    if (send_api_command(container_pid, json_cmd) == 0) {
        // Add to our tracking
        it->second.port_forwards.push_back({host_port, container_port});
        std::cout << "Port forward added: " << host_port << " -> " << container_port << std::endl;
        return 0;
    }

    return -1;
}

int Network::remove_port_forward(const pid_t& container_pid, int host_port) {
    auto it = container_networks.find(container_pid);
    if (it == container_networks.end()) {
        std::cerr << "Container network not found for PID " << container_pid << std::endl;
        return -1;
    }

    std::string json_cmd = "{\"execute\": \"remove_hostfwd\", \"arguments\": {\"proto\": \"tcp\", \"host_port\": "
                          + std::to_string(host_port) + "}}";

    if (send_api_command(container_pid, json_cmd) == 0) {
        // Remove from our tracking
        auto& forwards = it->second.port_forwards;
        forwards.erase(
            std::remove_if(forwards.begin(), forwards.end(),
                          [host_port](const std::pair<int, int>& p) { return p.first == host_port; }),
            forwards.end());

        std::cout << "Port forward removed: " << host_port << std::endl;
        return 0;
    }

    return -1;
}

std::string Network::get_container_ip(const pid_t& container_pid) {
    // Default slirp4netns guest IP
    return "10.0.2.100";
}

std::string Network::get_api_socket_path(const pid_t& container_pid) {
    auto it = container_networks.find(container_pid);
    if (it != container_networks.end()) {
        return it->second.api_socket;
    }
    return "";
}

bool Network::ping_container(const pid_t& container_pid) {
    std::string container_ip = get_container_ip(container_pid);
    std::string ping_cmd = "ping -c 1 -W 1 " + container_ip + " > /dev/null 2>&1";

    return system(ping_cmd.c_str()) == 0;
}

int Network::send_api_command(const pid_t& container_pid, const std::string& json_command) {
    auto it = container_networks.find(container_pid);
    if (it == container_networks.end()) {
        return -1;
    }

    std::string socket_path = it->second.api_socket;

    // Create Unix socket
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "Failed to connect to API socket: " << strerror(errno) << std::endl;
        close(sock);
        return -1;
    }

    // Send command
    ssize_t sent = send(sock, json_command.c_str(), json_command.length(), 0);
    if (sent == -1) {
        std::cerr << "Failed to send API command" << std::endl;
        close(sock);
        return -1;
    }

    // Read response (optional - for debugging)
    char response[1024];
    ssize_t received = recv(sock, response, sizeof(response) - 1, 0);
    if (received > 0) {
        response[received] = '\0';
        std::cout << "API Response: " << response << std::endl;
    }

    close(sock);
    return 0;
}

int Network::cleanup_networking(const pid_t& container_pid) {
    auto it = container_networks.find(container_pid);
    if (it == container_networks.end()) {
        return 0;  // Already cleaned up
    }

    pid_t slirp_pid = it->second.slirp_pid;
    std::string api_socket = it->second.api_socket;

    // Terminate slirp4netns process
    if (slirp_pid > 0) {
        kill(slirp_pid, SIGTERM);

        // Wait for process to terminate
        int status;
        int result = waitpid(slirp_pid, &status, WNOHANG);
        if (result == 0) {
            // Process still running, force kill
            sleep(1);
            kill(slirp_pid, SIGKILL);
            waitpid(slirp_pid, &status, 0);
        }
    }

    // Remove API socket file
    unlink(api_socket.c_str());

    // Remove from tracking
    container_networks.erase(it);

    std::cout << "Network cleanup complete for container " << container_pid << std::endl;
    return 0;
}

// Helper function implementations
int Network::wait_for_api_socket(const std::string& socket_path, int timeout_ms) {
    int elapsed = 0;
    const int check_interval = 100;  // 100ms

    while (elapsed < timeout_ms) {
        if (access(socket_path.c_str(), F_OK) == 0) {
            // Socket exists, wait a bit more for it to be ready
            usleep(200000);  // 200ms
            return 0;
        }
        usleep(check_interval * 1000);  // Convert to microseconds
        elapsed += check_interval;
    }

    return -1;  // Timeout
}

int Network::create_port_forward_args(const std::vector<std::pair<int, int>>& forwards,
                                    std::vector<std::string>& args) {
    for (const auto& forward : forwards) {
        args.push_back("--port-forward");
        args.push_back("tcp:" + std::to_string(forward.first) + ":" + std::to_string(forward.second));
    }
    return 0;
}

Network::~Network() {
    // Cleanup all container networks on destruction
    for (const auto& pair : container_networks) {
        cleanup_networking(pair.first);
    }
}
