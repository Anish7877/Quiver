#include "pasta_network.hpp"
#include "oci_runtime.hpp"
#include "utils.hpp"
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

auto PastaNetwork::setup_networking(pid_t container_pid, const OCIRuntime::Network& networks) -> pid_t {
	fs::path pasta_path{Utils::find_program_path("pasta")};
	if (pasta_path.empty()) {
		std::cerr << "Network Error: 'pasta' executable not found on host.\n";
		return -1;
	}
	std::string container_pid_str{std::to_string(container_pid)};
	std::string pidfile{std::format("/tmp/pasta-{}.pid", container_pid)};
	std::string logfile{std::format("/tmp/pasta-{}.log", container_pid)};

	unlink(pidfile.c_str());

	std::vector<char*> c_args{};
	c_args.emplace_back(const_cast<char*>(pasta_path.c_str()));
	c_args.emplace_back(const_cast<char*>("--config-net"));
	c_args.emplace_back(const_cast<char*>("--pid"));
	c_args.emplace_back(const_cast<char*>(pidfile.c_str()));

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

	pid_t launcher{fork()};
	if (launcher == -1) [[unlikely]] {
		std::cerr << std::format("Network Error: pasta fork failed -> '{}'.\n", std::strerror(errno)) << '\n';
		return -1;
	}
	if (launcher == 0) {
		if (setsid() == -1) {
			std::cerr << std::format("Network Error: setsid failed -> '{}'.\n", std::strerror(errno)) << '\n';
			_exit(EXIT_FAILURE);
		}
		int null_fd{open("/dev/null", O_RDWR)};
		if (null_fd != -1) {
			dup2(null_fd, STDIN_FILENO);
			dup2(null_fd, STDOUT_FILENO);
			close(null_fd);
		}
		int log_fd{open(logfile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600)};
		if (log_fd != -1) {
			dup2(log_fd, STDERR_FILENO);
			close(log_fd);
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

	auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds(10)};
	for (;;) {
		int status{};
		pid_t rc{waitpid(launcher, &status, WNOHANG)};
		if (rc == launcher) {
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
				std::cerr << std::format("Network Error: pasta launcher exited abnormally; see '{}'.\n", logfile);
				return -1;
			}
			break;
		}
		if (rc == -1) {
			if (errno == EINTR) continue;
			std::cerr << std::format("Network Error: waitpid failed -> '{}'.\n", std::strerror(errno));
			return -1;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			kill(launcher, SIGKILL);
			waitpid(launcher, nullptr, 0);
			std::cerr << "Network Error: pasta launcher timed out after 10 seconds.\n";
			return -1;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	pid_t daemon_pid{-1};
	{
		std::ifstream pf{pidfile};
		if (!pf || !(pf >> daemon_pid) || daemon_pid <= 1) {
			std::cerr << std::format("Network Error: failed to read daemon PID from '{}'.\n", pidfile);
			return -1;
		}
	}
	return daemon_pid;
}
