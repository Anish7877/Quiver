#include "command_line_handler.hpp"
#include "cgroups_manager_creator.hpp"
#include "spec_generator.hpp"
#include "utils.hpp"
#include "container_db_manager.hpp"
#include "image_manager.hpp"
#include "container_monitor.hpp"
#include "utils.hpp"
#include <chrono>
#include <csignal>
#include <exception>
#include <filesystem>
#include <format>
#include <nlohmann/detail/input/input_adapters.hpp>
#include <stdexcept>
#include <thread>
#include <vector>
#include <span>
#include <string>
#include <sstream>
#include <fstream>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>

static std::vector<std::string> split_string(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
                tokens.push_back(token);
        }
        return tokens;
}

auto CommandLineHandler::run(std::span<std::string> args) -> void {
        if (args.empty()) [[unlikely]] {
                throw std::runtime_error("Image name not found");
        }

        auto container_id{Utils::generate_container_id()};

        std::string image_name{};
        std::string container_name{};
        std::vector<std::string> commands{};
        size_t positional_start{args.size()};

        for (size_t i{0}; i < args.size(); ++i) {
                if (args[i] == "--") {
                        positional_start = i + 1;
                        break;
                }
                if (!args[i].empty() && args[i][0] != '-') {
                        positional_start = i;
                        break;
                }

                const std::string& arg = args[i];
                if (arg == "--name" || arg == "--hostname" || arg == "--domainname" ||
                    arg == "-u" || arg == "--user" || arg == "-w" || arg == "--workdir" ||
                    arg == "-e" || arg == "--env" || arg == "--env-file" ||
                    arg == "-v" || arg == "--volume" || arg == "--mount" || arg == "--tmpfs" ||
                    arg == "--rootfs-propagation" || arg == "-p" || arg == "--publish" ||
                    arg == "--device" || arg == "--cdi" || arg == "--cap-add" || arg == "--cap-drop" ||
                    arg == "--security-opt" || arg == "--ulimit" || arg == "--oom-score-adj" ||
                    arg == "--cpu-policy" || arg == "--cpu-priority" || arg == "--cpu-nice" ||
                    arg == "--cpu-rt-runtime" || arg == "--cpu-rt-period" || arg == "--cpu-scheduler-flags" ||
                    arg == "--pid" || arg == "--net" || arg == "--ipc" || arg == "--uts" ||
                    arg == "--mount-ns" || arg == "--time" || arg == "--cgroup" || arg == "--time-offset" ||
                    arg == "--mask" || arg == "--read-only-path" || arg == "--console-width" ||
                    arg == "--console-height" || arg == "--cgroup-path") {
                        ++i;
                }
        }

        if (positional_start < args.size()) {
                image_name = args[positional_start];
                for (size_t i = positional_start + 1; i < args.size(); ++i) {
                        commands.push_back(args[i]);
                }
        } else {
                image_name = args.back();
        }

        auto& image_manager{ImageManager::get_instance()};
        std::string outpath{Utils::get_image_path(image_name).string()};
        std::string error{};

        if (!Utils::file_exists(fs::path(outpath) / "config.json")) {
                std::cout << std::format("Unable to find image '{}' locally. Pulling...\n", image_name);
                image_manager.pull(image_name, outpath, error);

                if (!error.empty()) {
                        throw std::runtime_error(std::format("Failed to pull image with error '{}'\n", error));
                }
        }
        auto container_config{SpecGenerator::generate_default_rootless_spec(container_id, Utils::get_image_path(image_name))};

        for (size_t i = 0; i < positional_start; ++i) {
                const auto& arg = args[i];


                if (arg == "--name") {
                        if (++i < positional_start) {
                                container_name = args[i];
                        }
                }
                else if (arg == "-i" || arg == "--interactive" || arg == "-t" || arg == "--tty") {
                        container_config.terminal.value = true;
                } else if (arg == "-d" || arg == "--detach") {
                        container_config.detach.value = true;
                } else if (arg == "-u" || arg == "--user") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], ':');
                                container_config.user.uid = std::stoi(tokens[0]);
                                if (tokens.size() > 1) container_config.user.gid = std::stoi(tokens[1]);
                        }
                } else if (arg == "-w" || arg == "--workdir") {
                        if (++i < positional_start) container_config.cwd.value = args[i];
                } else if (arg == "-e" || arg == "--env") {
                        if (++i < positional_start) container_config.env.value.push_back(args[i]);
                } else if (arg == "--env-file") {
                        if (++i < positional_start) {
                                std::ifstream file(args[i]);
                                std::string line;
                                while (std::getline(file, line)) {
                                        if (!line.empty() && line[0] != '#') container_config.env.value.push_back(line);
                                }
                        }
                } else if (arg == "--hostname") {
                        if (++i < positional_start) container_config.hostname = args[i];
                } else if (arg == "--domainname") {
                        if (++i < positional_start) container_config.domain_name = args[i];
                }

                else if (arg == "-v" || arg == "--volume" || arg == "--mount") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], ':');
                                if (tokens.size() >= 2) {
                                        OCIRuntime::Mount mnt;
                                        mnt.source = tokens[0];
                                        mnt.destination = tokens[1];
                                        mnt.type = "bind";
                                        mnt.options.push_back("rbind");
                                        container_config.mounts.push_back(mnt);
                                }
                        }
                } else if (arg == "--tmpfs") {
                        if (++i < positional_start) {
                                OCIRuntime::Mount mnt;
                                mnt.destination = args[i];
                                mnt.type = "tmpfs";
                                container_config.mounts.push_back(mnt);
                        }
                } else if (arg == "--read-only") {
                        container_config.rootfs.read_only = true;
                } else if (arg == "--rootfs-propagation") {
                        if (++i < positional_start) container_config.rootfs_propagation.type = args[i];
                }


                else if (arg == "-p" || arg == "--publish") {
                        if (++i < positional_start) {
                                if (args[i].find("/udp") != std::string::npos) {
                                        container_config.networks.udp_ports.push_back(args[i]);
                                } else {
                                        container_config.networks.tcp_ports.push_back(args[i]);
                                }
                        }
                } else if (arg == "-P" || arg == "--publish-all") {
                        container_config.networks.auto_tcp = true;
                        container_config.networks.auto_udp = true;
                }


                else if (arg == "--device" || arg == "--cdi") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], ':');
                                OCIRuntime::Device dev;
                                dev.host_path = tokens[0];
                                dev.container_path = (tokens.size() > 1) ? tokens[1] : tokens[0];
                                container_config.devices.push_back(dev);
                        }
                }


                else if (arg == "--cap-add") {
                        if (++i < positional_start) {
                                container_config.capabilities.bounding.push_back(args[i]);
                                container_config.capabilities.effective.push_back(args[i]);
                                container_config.capabilities.permitted.push_back(args[i]);
                        }
                } else if (arg == "--cap-drop") {
                        if (++i < positional_start) {
                                const std::string& cap_to_drop = args[i];
                                std::erase(container_config.capabilities.bounding, cap_to_drop);
                                std::erase(container_config.capabilities.effective, cap_to_drop);
                                std::erase(container_config.capabilities.inheritable, cap_to_drop);
                                std::erase(container_config.capabilities.permitted, cap_to_drop);
                                std::erase(container_config.capabilities.ambient, cap_to_drop);
                        }
                } else if (arg == "--security-opt") {
                        if (++i < positional_start) {
                                if (args[i] == "no-new-privileges") {
                                        container_config.no_new_privileges.value = true;
                                } else if (args[i] == "seccomp=unconfined") {
                                        container_config.seccomp.default_action = "SCMP_ACT_ALLOW";
                                } else if (args[i].starts_with("seccomp=")) {
                                        std::string profile_path = args[i].substr(8);
                                        container_config.seccomp = Utils::load_seccomp_profile(profile_path);
                                }
                        }
                }

                else if (arg == "--ulimit") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], '=');
                                if (tokens.size() == 2) {
                                        auto limits = split_string(tokens[1], ':');
                                        OCIRuntime::RLimit rlimit;
                                        rlimit.name = tokens[0];
                                        rlimit.soft_limit = std::stoull(limits[0]);
                                        rlimit.hard_limit = (limits.size() > 1) ? std::stoull(limits[1]) : rlimit.soft_limit;
                                        container_config.rlimits.push_back(rlimit);
                                }
                        }
                } else if (arg == "--oom-score-adj") {
                        if (++i < positional_start) container_config.oom_score.value = std::stoi(args[i]);
                }

                else if (arg == "--cpu-policy") {
                        if (++i < positional_start) container_config.schedular_opts.policy = args[i];
                } else if (arg == "--cpu-priority") {
                        if (++i < positional_start) container_config.schedular_opts.priority = std::stoi(args[i]);
                } else if (arg == "--cpu-nice") {
                        if (++i < positional_start) container_config.schedular_opts.nice = std::stoi(args[i]);
                } else if (arg == "--cpu-rt-runtime") {
                        if (++i < positional_start) container_config.schedular_opts.runtime = std::stoull(args[i]);
                } else if (arg == "--cpu-rt-period") {
                        if (++i < positional_start) container_config.schedular_opts.period = std::stoull(args[i]);
                } else if (arg == "--cpu-scheduler-flags") {
                        if (++i < positional_start) container_config.schedular_opts.flags.push_back(args[i]);
                }

                else if (arg == "--pid" || arg == "--net" || arg == "--ipc" || arg == "--uts" ||
                                arg == "--mount-ns" || arg == "--time" || arg == "--cgroup") {
                        if (++i < positional_start) {
                                OCIRuntime::Namespace ns;
                                ns.type = arg.substr(2);
                                ns.path = args[i];
                                container_config.namespaces.push_back(ns);
                        }
                }

                else if (arg == "--time-offset") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], '=');
                                if (tokens.size() == 2) {
                                        OCIRuntime::TimeOffset offset;
                                        offset.type = tokens[0];
                                        offset.secs = std::stoll(tokens[1]);
                                        container_config.timeoffsets.push_back(offset);
                                }
                        }
                }

                else if (arg == "--mask") {
                        if (++i < positional_start) container_config.masked_paths.paths.push_back(args[i]);
                } else if (arg == "--read-only-path") {
                        if (++i < positional_start) container_config.read_only_paths.paths.push_back(args[i]);
                }

                else if (arg == "--console-width") {
                        if (++i < positional_start) container_config.console_size.width = std::stoi(args[i]);
                } else if (arg == "--console-height") {
                        if (++i < positional_start) container_config.console_size.height = std::stoi(args[i]);
                }

                else if (arg == "--cgroup-path") {
                        if (++i < positional_start) container_config.cgroups_path = args[i];
                }

                else {
                        throw std::runtime_error("Unknown option: " + arg);
                }
        }

        if (commands.empty()) {
                fs::path config_path = fs::path(outpath) / "config.json";
                if (Utils::file_exists(config_path)) {
                        std::ifstream config_file(config_path);
                        nlohmann::json img_config;
                        config_file >> img_config;

                        if (img_config.contains("config")) {
                                auto& cfg = img_config["config"];

                                if (commands.empty()) {
                                        if (cfg.contains("Entrypoint") && !cfg["Entrypoint"].is_null()) {
                                                for (const auto& item : cfg["Entrypoint"]) {
                                                        commands.push_back(item.get<std::string>());
                                                }
                                        }
                                        if (cfg.contains("Cmd") && !cfg["Cmd"].is_null()) {
                                                for (const auto& item : cfg["Cmd"]) {
                                                        commands.push_back(item.get<std::string>());
                                                }
                                        }
                                }

                                if (cfg.contains("Env") && !cfg["Env"].is_null()) {
                                        for (const auto& item : cfg["Env"]) {
                                                container_config.env.value.push_back(item.get<std::string>());
                                        }
                                }

                                if (cfg.contains("WorkingDir") && !cfg["WorkingDir"].is_null()) {
                                        std::string wd = cfg["WorkingDir"].get<std::string>();
                                        if (!wd.empty()) {
                                                container_config.cwd.value = wd;
                                        }
                                }
                        }
                }
        }
        if (!commands.empty()) {
                container_config.args.value = commands;
        }

        container_config.container_id = container_id;
        auto& container_monitor{ContainerMonitor::get_instance()};
        bool is_terminal{container_config.terminal.value};
        bool is_detached{container_config.detach.value};
        container_config.rootfs.path = std::move(outpath);
        container_monitor.init(container_config, image_name, container_name, true);
        container_monitor.invoke_container();
        if (is_terminal && !is_detached) {
                container_monitor.attach_to_container(container_id);
        }
}

auto CommandLineHandler::ps(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                container_db_manager.list_all_running_container();
        }
        else {
                if (args.size() != 1) [[unlikely]] {
                        std::cerr << "Error: more than one argument provided\n";
                        Utils::print_usage();
                        return;
                }
                if (args[0] == "-a") {
                        container_db_manager.list_all_container();
                }
                else {
                        std::cerr << std::format("Error: Unknown argument '{}'\n", args[0]);
                        Utils::print_usage();
                }
        }
}

auto CommandLineHandler::remove(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }
        for (const auto& arg : args) {
                container_db_manager.remove_container(arg);
        }
}

auto CommandLineHandler::inspect(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }
        if (args.size() != 1) [[unlikely]] {
                std::cerr << "Error: More than one argument provided\n";
                Utils::print_usage();
                return;
        }
        container_db_manager.inspect_container(args.front());
}

auto CommandLineHandler::pause(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }
        for (const auto& arg : args) {
                auto container{container_db_manager.get_container(args.front())};
                if (container && container->status == "running") {
                        auto cgroup_manager{CGroupsManagerCreator::create_cgourps_manager(arg, container->config.cgroups_path)};
                        cgroup_manager->set_freeze("1");
                        container->status = "paused";
                        container_db_manager.update_container(container->config.container_id, container.value());
                }
                else {
                        std::cerr << std::format("Error: Container '{}' not found or Container not running\n", arg);
                }
        }
}

auto CommandLineHandler::unpause(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }
        for (const auto& arg : args) {
                auto container{container_db_manager.get_container(args.front())};
                if (container && container->status == "paused") {
                        auto cgroup_manager{CGroupsManagerCreator::create_cgourps_manager(arg, container->config.cgroups_path)};
                        cgroup_manager->set_freeze("0");
                        container->status = "running";
                        container_db_manager.update_container(container->config.container_id, container.value());
                }
                else {
                        std::cerr << std::format("Error: Container '{}' not found or Container is not paused\n", arg);
                }
        }
}

auto CommandLineHandler::attach(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }
        if (args.size() != 1) [[unlikely]] {
                std::cerr << "Error: More than one argument provided\n";
                Utils::print_usage();
                return;
        }
        auto container{container_db_manager.get_container(args.front())};
        if (container) {
                auto& container_monitor{ContainerMonitor::get_instance()};
                container_monitor.init(container->config, container->image, container->name, false);
                container_monitor.attach_to_container(args.front());
        }
        else {
                std::cerr << std::format("Error: Container '{}' not found\n", args.front());
        }
}

auto CommandLineHandler::ports(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (!args.empty()) [[unlikely]] {
                std::cerr << "Error: Arguments Provided\n";
                Utils::print_usage();
                return;
        }
        auto containers{container_db_manager.get_all_container()};
        auto chunk_ports = [](const std::vector<std::string>& ports, size_t chunk_size = 3) {
                std::vector<std::string> lines;
                if (ports.empty()) {
                        lines.push_back("-");
                        return lines;
                }

                for (size_t i = 0; i < ports.size(); i += chunk_size) {
                        std::string line = "";
                        for (size_t j = 0; j < chunk_size && (i + j) < ports.size(); ++j) {
                                line += ports[i + j];
                                if (j < chunk_size - 1 && (i + j) < ports.size() - 1) {
                                        line += ", ";
                                }
                        }
                        lines.push_back(line);
                }
                return lines;
        };
        std::cout << std::format("{:<70} {:<45} {}\n", "Container ID", "tcp_port", "udp_port");
        for (const auto& container : containers) {
                auto tcp_lines{chunk_ports(container.config.networks.tcp_ports, 3)};
                auto udp_lines{chunk_ports(container.config.networks.udp_ports, 3)};
                size_t max_lines{std::max(tcp_lines.size(), udp_lines.size())};

                for (size_t i{0}; i < max_lines; ++i) {
                        std::string current_id = (i == 0) ? container.config.container_id : "";

                        std::string t_port{(i < tcp_lines.size()) ? tcp_lines[i] : ""};
                        std::string u_port{(i < udp_lines.size()) ? udp_lines[i] : ""};

                        std::cout << std::format("{:<70} {:<45} {}\n", current_id, t_port, u_port);
                }
        }
}

auto CommandLineHandler::start(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        auto& container_monitor{ContainerMonitor::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }
        for (const auto& arg : args) {
                auto container{container_db_manager.get_container(arg)};
                if (container && container->status != "running") {
                        container_monitor.init(container->config, container->image, container->name, false);
                        container_monitor.invoke_container();
                }
                else {
                        std::cerr << std::format("Error: Container '{}' not found or Container already running\n", arg);
                }
        }
}

auto CommandLineHandler::stop(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        auto& container_monitor{ContainerMonitor::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }
        for (const auto& arg : args) {
                auto container{container_db_manager.get_container(arg)};
                if (container && container->status == "running") {
                        if (kill(container->config.pid, SIGTERM) == 0) {
                                bool stopped{false};
                                for (int i{0}; i<100; ++i) {
                                        if (!Utils::is_process_alive(container->config.pid, container->config.container_id)) {
                                                stopped = true;
                                                break;
                                        }
                                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                }
                                if (!stopped) {
                                        std::cerr << std::format("WARN: Container '{}' timed out. Sending SIGKILL...\n", arg);
                                        kill(container->config.pid, SIGKILL);
                                }
                                container->status = "stopped";
                                container_db_manager.update_container(arg, container.value());
                                std::cout << std::format("Container '{}' stopped successfully.\n", arg);
                        }
                }
                else {
                        std::cerr << std::format("Error: Container '{}' not found or Container is not running\n", arg);
                }
        }
}

auto CommandLineHandler::prune(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (!args.empty()) [[unlikely]] {
                std::cerr << "Error: Arguments Provided\n";
                Utils::print_usage();
                return;
        }
        auto containers{container_db_manager.get_all_container()};
        for (const auto& container : containers) {
                if (container.status != "running") {
                        container_db_manager.remove_container(container.config.container_id);
                }
        }
}

auto CommandLineHandler::cp(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();

        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided.\n";
                Utils::print_usage();
                return;
        }

        bool is_recursive{false};
        size_t start_idx{0};

        if (args.front() == "-r") {
                is_recursive = true;
                start_idx = 1;
        }

        if (args.size() - start_idx != 3) [[unlikely]] {
                std::cerr << "Error: Invalid number of arguments provided.\n";
                Utils::print_usage();
                return;
        }

        auto container_id{args[start_idx]};
        auto host_path{args[start_idx + 1]};
        std::string container_path{args[start_idx + 2]};

        auto container{container_db_manager.get_container(container_id)};
        if (!container) {
                std::cerr << std::format("Error: Container '{}' not found.\n", container_id);
                return;
        }

        if (!fs::exists(host_path)) [[unlikely]] {
                std::cerr << std::format("Error: Host path '{}' doesn't exist.\n", host_path);
                return;
        }

        if (!container_path.empty() && container_path.front() == '/') {
                container_path.erase(0, 1);
        }

        fs::path base_dir{Utils::get_base_dir()};
        fs::path final_container_path;

        if (container->config.vfs) {
                final_container_path = base_dir / "vfs" / std::format("quiver_{}", container->config.container_id) / container_path;
        } else {
                final_container_path = base_dir / "filesystems" / std::format("quiver_{}", container->config.container_id) / "upper_dir" / container_path;
        }

        if (!is_recursive && !fs::is_directory(host_path)) {
                if (container_path.empty() || container_path.back() == '/' || fs::is_directory(final_container_path)) {
                        final_container_path /= fs::path(host_path).filename();
                }
        }

        try {
                Utils::ensure_dir(final_container_path.parent_path());
        }
        catch (const std::exception& e) {
                std::cerr << std::format("Error creating container directories: {}\n", e.what());
                return;
        }

        if (is_recursive) {
                try {
                        Utils::copy_directory(host_path, final_container_path);
                        std::cout << std::format("Successfully copied directory '{}' to container '{}'.\n", host_path, container_id);
                }
                catch (const std::exception& e) {
                        std::cerr << std::format("Error copying directory: {}\n", e.what());
                }
        }
        else {
                if (fs::is_directory(host_path)) {
                        std::cerr << std::format("Error: '{}' is a directory. Use -r to copy directories.\n", host_path);
                }
                else {
                        std::error_code ec{};
                        fs::copy(host_path, final_container_path, fs::copy_options::overwrite_existing, ec);

                        if (ec) {
                                std::cerr << std::format("Error: Unable to copy file -> {}\n", ec.message());
                        } else {
                                std::cout << std::format("Successfully copied file '{}' to container '{}'.\n", host_path, container_id);
                        }
                }
        }
}

auto CommandLineHandler::stats(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();

        if (!args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided. Please specify a container ID.\n";
                Utils::print_usage();
                return;
        }

        auto read_cgroup_val{[](const fs::path& path) -> uint64_t {
                std::ifstream file(path);
                uint64_t val{0};
                if (file >> val) return val;
                return 0;
        }};

        auto get_cpu_usage_usec{[](const fs::path& path) -> uint64_t {
                std::ifstream file(path);
                std::string key;
                uint64_t val{0};
                while (file >> key >> val) {
                        if (key == "usage_usec") return val;
                }
                return 0;
        }};

        auto get_cgroup_path{[](pid_t pid) -> std::optional<fs::path> {
                std::ifstream file(std::format("/proc/{}/cgroup", pid));
                std::string line;

                while (std::getline(file, line)) {
                        size_t first_colon = line.find(':');
                        if (first_colon != std::string::npos) {
                                size_t second_colon = line.find(':', first_colon + 1);
                                if (second_colon != std::string::npos) {
                                        std::string cg_path = line.substr(second_colon + 1);

                                        if (!cg_path.empty() && cg_path.front() == '/') {
                                                cg_path.erase(0, 1);
                                        }

                                        return fs::path("/sys/fs/cgroup") / cg_path;
                                }
                        }
                }
                return std::nullopt;
        }};

        std::cout << std::format("{:<70} {:<10} {:<15} {:<10}\n", "CONTAINER ID", "CPU %", "MEM USAGE", "PIDS");
        auto containers{container_db_manager.get_all_container()};
        for (const auto& container : containers) {
                if (container.status != "running") {
                        continue;
                }

                auto cgroup_base_opt{get_cgroup_path(container.config.pid)};
                if (!cgroup_base_opt) {
                        std::cerr << std::format("Error: Could not read /proc/{}/cgroup for container '{}'.\n", container.config.pid, container.config.container_id);
                        continue;
                }

                fs::path cgroup_base{cgroup_base_opt.value()};

                if (!fs::exists(cgroup_base)) {
                        std::cerr << std::format("Error: Resolved Cgroup path '{}' does not exist.\n", cgroup_base.string());
                        continue;
                }

                uint64_t mem_bytes{read_cgroup_val(cgroup_base / "memory.current")};
                uint64_t pids{read_cgroup_val(cgroup_base / "pids.current")};

                double mem_mb{static_cast<double>(mem_bytes) / (1024.0 * 1024.0)};

                fs::path cpu_stat_path{cgroup_base / "cpu.stat"};

                uint64_t cpu_start{get_cpu_usage_usec(cpu_stat_path)};
                auto time_start{std::chrono::steady_clock::now()};

                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                uint64_t cpu_end{get_cpu_usage_usec(cpu_stat_path)};
                auto time_end{std::chrono::steady_clock::now()};

                double cpu_percent{0.0};
                if (cpu_end > cpu_start) {
                        auto real_time_delta_usec{std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_start).count()};
                        uint64_t cpu_time_delta_usec{cpu_end - cpu_start};
                        cpu_percent = (static_cast<double>(cpu_time_delta_usec) / static_cast<double>(real_time_delta_usec)) * 100.0;
                }

                auto cpu_str{std::format("{:.2f}%", cpu_percent)};
                auto mem_str{std::format("{:.2f}MB", mem_mb)};
                std::cout << std::format("{:<70} {:<10} {:<15} {:<10}\n", container.config.container_id, cpu_str, mem_str, pids);
        }
}
