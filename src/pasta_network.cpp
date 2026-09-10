#include "pasta_network.hpp"
#include "oci_runtime.hpp"
#include "utils.hpp"
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <unistd.h>
#include <fcntl.h>

auto PastaNetwork::setup_networking(pid_t container_pid, const OCIRuntime::Network& networks) -> pid_t {
        fs::path pasta_path{Utils::find_program_path("pasta")};
        if (pasta_path.empty()) {
                std::cerr << "Network Error: 'pasta' executable not found on host.\n";
                return -1;
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
                std::cerr << std::format("Network Error: pasta fork failed -> '{}'.\n", std::strerror(errno)) << '\n';
                return -1;
        }
        if (pasta_pid == 0) {
                if (setsid() == -1) {
                        std::cerr << std::format("Network Error: setsid failed -> '{}'.\n", std::strerror(errno)) << '\n';
                        _exit(EXIT_FAILURE);
                }
                int null_fd{open("/dev/null", O_RDWR)};
                if (null_fd != -1) {
                        dup2(null_fd, STDIN_FILENO);
                        dup2(null_fd, STDOUT_FILENO);
                        dup2(null_fd, STDERR_FILENO);
                        close(null_fd);
                }
                
                long max_fd{sysconf(_SC_OPEN_MAX)};
                if (max_fd == -1) max_fd = 1024;
                for (int fd{3}; fd < max_fd; ++fd) {
                        close(fd);
                }

                execv(pasta_path.c_str(), c_args.data());
                std::cerr << std::format("Network Fatal: execv failed for pasta -> '{}'.\n", std::strerror(errno));
                _exit(EXIT_FAILURE);
        }
        return pasta_pid;
}
