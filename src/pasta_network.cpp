#include "pasta_network.hpp"
#include "oci_runtime.hpp"
#include "utils.hpp"
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <unistd.h>

auto PastaNetwork::setup_networking(pid_t container_pid) -> void {
        OCIRuntime::Network network_config{};
        network_config.auto_tcp = false;
        network_config.auto_udp = false;
        setup_networking_with_ports(container_pid, network_config);
}

auto PastaNetwork::setup_networking_with_ports(pid_t container_pid, const OCIRuntime::Network& networks) -> void {
        fs::path pasta_path{Utils::find_program_path("pasta")};
        if (pasta_path.empty()) {
                std::cerr << "Network Error: 'pasta' executable not found on host.\n";
                return;
        }
        std::string container_pid_str{std::to_string(container_pid)};

        std::vector<char*> c_args{};
        c_args.emplace_back(const_cast<char*>(pasta_path.c_str()));
        c_args.emplace_back(const_cast<char*>("--config-net"));

        if (networks.auto_tcp) {
                c_args.emplace_back(const_cast<char*>("-t"));
                c_args.emplace_back(const_cast<char*>("auto"));
        }
        for (const auto& tcp_port : networks.tcp_ports) {
                c_args.emplace_back(const_cast<char*>("-t"));
                c_args.emplace_back(const_cast<char*>(tcp_port.c_str()));
        }

        if (networks.auto_udp) {
                c_args.emplace_back(const_cast<char*>("-u"));
                c_args.emplace_back(const_cast<char*>("auto"));
        }
        for (const auto& udp_port : networks.udp_ports) {
                c_args.emplace_back(const_cast<char*>("-u"));
                c_args.emplace_back(const_cast<char*>(udp_port.c_str()));
        }

        c_args.emplace_back(const_cast<char*>(container_pid_str.c_str()));
        c_args.emplace_back(nullptr);

        pid_t pasta_pid{fork()};
        if (pasta_pid == -1) [[unlikely]] {
                std::cerr << std::format("Network Error: pasta fork failed -> '{}'.", std::strerror(errno)) << '\n';
                return;
        }
        if (pasta_pid == 0) {
                if (setsid() == -1) {
                        std::cerr << std::format("Network Error: setsid failed -> '{}'.", std::strerror(errno)) << '\n';
                        _exit(EXIT_FAILURE);
                }
                execv(pasta_path.c_str(), c_args.data());
                std::cerr << std::format("Network Fatal: execv failed for pasta -> '{}'.\n", std::strerror(errno));
                _exit(EXIT_FAILURE);
        }
        return;
}
