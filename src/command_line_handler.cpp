#include "command_line_handler.hpp"
#include "cgroups_manager_creator.hpp"
#include "cgroups_manager_interface.hpp"
#include "spec_generator.hpp"
#include "utils.hpp"
#include "container_db_manager.hpp"
#include "image_manager.hpp"
#include "container_config_parser.hpp"
#include "container_monitor.hpp"
#include "utils.hpp"
#include "types.hpp"
#include "process_lock.hpp"
#include "image_db_manager.hpp"
#include <chrono>
#include <csignal>
#include <exception>
#include <filesystem>
#include <format>
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
#include <pwd.h>
#include <chrono>
#include <set>
#include <pty.h>
#include <termios.h>
#include <poll.h>

static std::vector<std::string> split_string(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
                tokens.push_back(token);
        }
        return tokens;
}

static auto ensure_buildkit() -> void {
        std::string uid{std::to_string(getuid())};
        fs::path run_dir{std::format("/run/user/{}/buildkit", uid)};
        fs::path socket_path{run_dir / "buildkitd.sock"};
        fs::path lock_path{run_dir / "quiver_buildkit.lock"};


        auto spawn_buildkit{[&]() {
                if (fs::exists(socket_path)) {
                        return;
                }
                fs::path log_dir{fs::path(Utils::get_base_dir()) / "logs"};
                fs::create_directories(log_dir);
                fs::path log_file{log_dir / "buildkitd.log"};
                std::vector<std::string> exec_args = {
                        "rootlesskit",
                        "buildkitd",
                        "--oci-worker-snapshotter=overlayfs"
                };
                std::vector<char*> c_args;
                c_args.reserve(exec_args.size() + 1);
                for (auto& arg : exec_args) c_args.push_back(arg.data());
                c_args.push_back(nullptr);

                pid_t pid{fork()};

                if (pid < 0) [[unlikely]] {
                        throw std::runtime_error("Failed to fork process for BuildKit buildkit.");
                }
                else if (pid == 0) {
                        setsid();

                        pid_t sid_pid{fork()};
                        if (sid_pid < 0) exit(EXIT_FAILURE);
                        if (sid_pid > 0) exit(EXIT_SUCCESS);


                        int log_fd{open(log_file.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644)};
                        if (log_fd != -1) {
                                dup2(log_fd, STDIN_FILENO);
                                dup2(log_fd, STDOUT_FILENO);
                                dup2(log_fd, STDERR_FILENO);
                                close(log_fd);
                        }

                        execvp(c_args[0], c_args.data());
                        exit(EXIT_FAILURE);
                }
                else {
                        int status;
                        waitpid(pid, &status, 0);

                        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                                for (int i{0}; i < 50; ++i) {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                        if (fs::exists(socket_path)) {
                                                return;
                                        }
                                }
                                throw std::runtime_error(std::format("Buildkit started, but socket not found. See logs: {}", log_file.string()));
                        } else {
                                throw std::runtime_error("Failed to launch BuildKit buildkit.");
                        }
                }
        }};

        try {
                ProcessLock lock(lock_path);
                spawn_buildkit();
        } catch (const std::exception& e) {
                std::cerr << std::format("Error during buildkit initialization: {}\n", e.what());
        }
}

auto CommandLineHandler::run(std::span<std::string> args) -> void {
        auto& image_db_manager{ImageDbManager::get_instance()};
        auto& container_monitor{ContainerMonitor::get_instance()};
        image_db_manager.init();
        if (args.empty()) [[unlikely]] {
                throw std::runtime_error("Image name not found");
        }

        auto container_id{Utils::generate_id()};

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
                    arg == "--console-height" || arg == "--cgroup-path" ||
                    arg == "--cpu-quota" || arg == "--cpu-period" || arg == "--cpu-weight" ||
                    arg == "--memory-max" || arg == "--memory-swap" || arg == "--pids-limit" ||
                    arg == "--cpuset-cpus" || arg == "--set-cpuset-mems" ||
                    arg == "--set-io-max" || arg == "--set-io-weight") {
                        if (++i >= args.size()) {
                                throw std::runtime_error("Missing argument for option: " + arg);
                        }
                }
        }

        if (positional_start < args.size()) {
                image_name = args[positional_start];
                for (size_t i = positional_start + 1; i < args.size(); ++i) {
                        commands.push_back(args[i]);
                }
        } else {
                throw std::runtime_error("Image name not found");
        }

        auto& image_manager{ImageManager::get_instance()};
        image_manager.init();
        std::string outpath{Utils::get_image_path(image_name).string()};
        std::string error{};

        if (!fs::exists(fs::path(outpath) / "config.json")) {
                std::cout << std::format("Unable to find image '{}' locally. Pulling...\n", image_name);
                image_manager.pull(image_name, outpath, error);
                if (!error.empty()) {
                        throw std::runtime_error(std::format("Failed to pull image with error '{}'\n", error));
                }
                std::string metadata_image_name{};
                std::string metadata_image_tag{};
                auto index{image_name.find(':')};
                if (index != std::string::npos) {
                        metadata_image_name = image_name.substr(0, index);
                        metadata_image_tag = image_name.substr(index + 1);
                } else {
                        metadata_image_name = image_name;
                        metadata_image_tag = "latest";
                }
                ImageMetadata image_metadata{};
                image_metadata.id = Utils::generate_id();
                image_metadata.name = metadata_image_name;
                image_metadata.tag = metadata_image_tag;
                image_metadata.size_bytes = 0;

                try {
                        auto dir_opts = fs::directory_options::skip_permission_denied;
                        for (const auto& entry : fs::recursive_directory_iterator(outpath, dir_opts)) {
                                if (entry.is_regular_file() && !entry.is_symlink()) {
                                        image_metadata.size_bytes += entry.file_size();
                                }
                        }
                } catch (const std::exception& e) {
                        std::cerr << "Warning: Could not accurately calculate total image size: " << e.what() << "\n";
                }

                image_metadata.source = "dockerhub";
                image_db_manager.add_image(image_metadata);
                std::cout << std::format("Successfully pulled Image '{}'\n", image_name);
        }

        auto container_config{SpecGenerator::generate_default_rootless_spec(container_id, Utils::get_image_path(image_name))};
        ContainerMonitor::Limits limits{};

        for (size_t i{0}; i < positional_start; ++i) {
                const auto& arg = args[i];
                try {
                if (arg == "--name") {
                        if (++i < positional_start) {
                                container_name = args[i];
                        }
                } else if (arg == "-i" || arg == "--interactive" || arg == "-t" || arg == "--tty") {
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
                                if (!file) throw std::runtime_error("Could not open env-file: " + args[i]);
                                std::string line;
                                while (std::getline(file, line)) {
                                        if (!line.empty() && line[0] != '#') container_config.env.value.push_back(line);
                                }
                        }
                } else if (arg == "--hostname") {
                        if (++i < positional_start) container_config.hostname = args[i];
                } else if (arg == "--domainname") {
                        if (++i < positional_start) container_config.domain_name = args[i];
                } else if (arg == "-v" || arg == "--volume" || arg == "--mount") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], ':');
                                if (tokens.size() >= 2) {
                                        OCIRuntime::Mount mnt;
                                        mnt.source = tokens[0];
                                        mnt.destination = tokens[1];
                                        mnt.type = "bind";
                                        mnt.options.push_back("rbind");
                                        if (tokens.size() >= 3) {
                                                if (tokens[2] == "ro") {
                                                        mnt.options.push_back("ro");
                                                } else if (tokens[2] == "rw") {
                                                        mnt.options.push_back("rw");
                                                }
                                        }

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
                } else if (arg == "-p" || arg == "--publish") {
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
                } else if (arg == "--cap-add") {
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
                                        if (limits.empty()) throw std::runtime_error("Invalid format for --ulimit");
                                        OCIRuntime::RLimit rlimit;
                                        rlimit.name = tokens[0];
                                        rlimit.soft_limit = std::stoull(limits[0]);
                                        rlimit.hard_limit = (limits.size() > 1) ? std::stoull(limits[1]) : rlimit.soft_limit;
                                        container_config.rlimits.push_back(rlimit);
                                }
                        }
                } else if (arg == "--oom-score-adj") {
                        if (++i < positional_start) container_config.oom_score.value = std::stoi(args[i]);
                } else if (arg == "--cpu-policy") {
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
                } else if (arg == "--pid" || arg == "--net" || arg == "--ipc" || arg == "--uts" ||
                                arg == "--mount-ns" || arg == "--time" || arg == "--cgroup") {
                        if (++i < positional_start) {
                                OCIRuntime::Namespace ns;
                                ns.type = arg.substr(2);
                                ns.path = args[i];
                                container_config.namespaces.push_back(ns);
                        }
                } else if (arg == "--time-offset") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], '=');
                                if (tokens.size() == 2) {
                                        OCIRuntime::TimeOffset offset;
                                        offset.type = tokens[0];
                                        offset.secs = std::stoll(tokens[1]);
                                        container_config.timeoffsets.push_back(offset);
                                }
                        }
                } else if (arg == "--mask") {
                        if (++i < positional_start) container_config.masked_paths.paths.push_back(args[i]);
                } else if (arg == "--read-only-path") {
                        if (++i < positional_start) container_config.read_only_paths.paths.push_back(args[i]);
                } else if (arg == "--console-width") {
                        if (++i < positional_start) container_config.console_size.width = std::stoi(args[i]);
                } else if (arg == "--console-height") {
                        if (++i < positional_start) container_config.console_size.height = std::stoi(args[i]);
                } else if (arg == "--cgroup-path") {
                        if (++i < positional_start) container_config.cgroups_path = args[i];
                } else if (arg == "--cpu-quota") {
                        if (++i < positional_start) limits.cpu_quota = std::stoi(args[i]);
                } else if (arg == "--cpu-period") {
                        if (++i < positional_start) limits.cpu_period = std::stoull(args[i]);
                } else if (arg == "--cpu-weight") {
                        if (++i < positional_start) limits.cpu_weight = std::stoull(args[i]);
                } else if (arg == "--memory-max") {
                        if (++i < positional_start) limits.memory_max = std::stoull(args[i]);
                } else if (arg == "--memory-swap") {
                        if (++i < positional_start) limits.memory_swap = std::stoull(args[i]);
                } else if (arg == "--pids-limit") {
                        if (++i < positional_start) limits.pids_limit = std::stoull(args[i]);
                } else if (arg == "--cpuset-cpus") {
                        if (++i < positional_start) limits.cpuset_cpus = args[i];
                } else if (arg == "--set-cpuset-mems") {
                        if (++i < positional_start) limits.cpuset_mems = args[i];
                } else if (arg == "--set-io-weight") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], ':');
                                if (tokens.size() == 3) {
                                        IOWeightUpdate iw{};
                                        iw.major = std::stoull(tokens[0]);
                                        iw.minor = std::stoull(tokens[1]);
                                        iw.weight = std::stoull(tokens[2]);
                                        limits.io_weight_updates.push_back(iw);
                                } else {
                                        std::cerr << "Warning: Invalid format for --set-io-weight. Expected MAJOR:MINOR:WEIGHT\n";
                                }
                        }
                } else if (arg == "--set-io-max") {
                        if (++i < positional_start) {
                                auto tokens = split_string(args[i], ':');
                                if (tokens.size() == 6) {
                                        IOMaxUpdate im{};
                                        im.major = std::stoull(tokens[0]);
                                        im.minor = std::stoull(tokens[1]);
                                        im.limits.rbps = std::stoull(tokens[2]);
                                        im.limits.wbps = std::stoull(tokens[3]);
                                        im.limits.riops = std::stoull(tokens[4]);
                                        im.limits.wiops = std::stoull(tokens[5]);
                                        limits.io_max_updates.push_back(im);
                                } else {
                                        std::cerr << "Warning: Invalid format for --set-io-max. Expected MAJOR:MINOR:RBPS:WBPS:RIOPS:WIOPS\n";
                                }
                        }
                } else {
                        throw std::runtime_error("Unknown option: " + arg);
                }
                } catch (const std::invalid_argument& e) {
                        throw std::runtime_error("Invalid numeric argument for option: " + arg);
                } catch (const std::out_of_range& e) {
                        throw std::runtime_error("Numeric argument out of range for option: " + arg);
                }
        }

        fs::path config_path{fs::path(outpath) / "config.json"};

        if (!Utils::file_exists(config_path)) {
                config_path = fs::path(outpath) / "image_config.json";
        }

        if (Utils::file_exists(config_path)) {
                std::ifstream config_file(config_path);
                nlohmann::json img_config;
                config_file >> img_config;

                if (img_config.contains("process")) {
                        auto& process_cfg = img_config["process"];

                        if (process_cfg.contains("terminal") && process_cfg["terminal"].is_boolean()) {
                                if (!container_config.terminal.value && !container_config.detach.value) {
                                        container_config.terminal.value = process_cfg["terminal"].get<bool>();
                                }
                        }

                        if (process_cfg.contains("user")) {
                                auto& user_cfg = process_cfg["user"];
                                if (user_cfg.contains("uid") && user_cfg["uid"].is_number()) {
                                        container_config.user.uid = user_cfg["uid"].get<uid_t>();
                                }
                                if (user_cfg.contains("gid") && user_cfg["gid"].is_number()) {
                                        container_config.user.gid = user_cfg["gid"].get<gid_t>();
                                }
                                if (user_cfg.contains("additionalGids") && user_cfg["additionalGids"].is_array()) {
                                        for (const auto& gid : user_cfg["additionalGids"]) {
                                                container_config.user.additional_gids.push_back(gid.get<gid_t>());
                                        }
                                }
                        }

                        if (commands.empty() && process_cfg.contains("args") && process_cfg["args"].is_array()) {
                                for (const auto& item : process_cfg["args"]) {
                                        commands.push_back(item.get<std::string>());
                                }
                        }

                        if (process_cfg.contains("env") && process_cfg["env"].is_array()) {
                                for (const auto& item : process_cfg["env"]) {
                                        container_config.env.value.push_back(item.get<std::string>());
                                }
                        }

                        if (process_cfg.contains("cwd") && process_cfg["cwd"].is_string()) {
                                if (container_config.cwd.value == "/") {
                                        container_config.cwd.value = process_cfg["cwd"].get<std::string>();
                                }
                        }

                        if (process_cfg.contains("capabilities")) {
                                auto& caps = process_cfg["capabilities"];
                                auto append_caps = [](const nlohmann::json& j, const std::string& key, std::vector<std::string>& out) {
                                        if (j.contains(key) && j[key].is_array()) {
                                                for (const auto& item : j[key]) {
                                                        std::string cap = item.get<std::string>();
                                                        if (std::find(out.begin(), out.end(), cap) == out.end()) {
                                                                out.push_back(cap);
                                                        }
                                                }
                                        }
                                };
                                append_caps(caps, "bounding", container_config.capabilities.bounding);
                                append_caps(caps, "effective", container_config.capabilities.effective);
                                append_caps(caps, "inheritable", container_config.capabilities.inheritable);
                                append_caps(caps, "permitted", container_config.capabilities.permitted);
                                append_caps(caps, "ambient", container_config.capabilities.ambient);
                        }

                        if (process_cfg.contains("rlimits") && process_cfg["rlimits"].is_array()) {
                                for (const auto& rl : process_cfg["rlimits"]) {
                                        OCIRuntime::RLimit rlimit{};
                                        if (rl.contains("type") && rl["type"].is_string()) {
                                                std::string r_type = rl["type"].get<std::string>();
                                                if (r_type.starts_with("RLIMIT_")) r_type = r_type.substr(7);
                                                rlimit.name = r_type;
                                        }
                                        if (rl.contains("hard") && rl["hard"].is_number()) rlimit.hard_limit = rl["hard"].get<uint64_t>();
                                        if (rl.contains("soft") && rl["soft"].is_number()) rlimit.soft_limit = rl["soft"].get<uint64_t>();
                                        container_config.rlimits.push_back(rlimit);
                                }
                        }

                        if (process_cfg.contains("noNewPrivileges") && process_cfg["noNewPrivileges"].is_boolean()) {
                                container_config.no_new_privileges.value = process_cfg["noNewPrivileges"].get<bool>();
                        }
                        if (process_cfg.contains("oomScoreAdj") && process_cfg["oomScoreAdj"].is_number()) {
                                container_config.oom_score.value = process_cfg["oomScoreAdj"].get<int>();
                        }
                }

                if (img_config.contains("root")) {
                        auto& root_cfg = img_config["root"];
                        if (root_cfg.contains("readonly") && root_cfg["readonly"].is_boolean()) {
                                container_config.rootfs.read_only = root_cfg["readonly"].get<bool>();
                        }
                }

                if (img_config.contains("mounts") && img_config["mounts"].is_array()) {
                        for (const auto& m : img_config["mounts"]) {
                                OCIRuntime::Mount mnt;
                                if (m.contains("destination") && m["destination"].is_string()) mnt.destination = m["destination"].get<std::string>();
                                if (m.contains("type") && m["type"].is_string()) mnt.type = m["type"].get<std::string>();
                                if (m.contains("source") && m["source"].is_string()) mnt.source = m["source"].get<std::string>();
                                if (m.contains("options") && m["options"].is_array()) {
                                        for (const auto& opt : m["options"]) {
                                                mnt.options.push_back(opt.get<std::string>());
                                        }
                                }
                                container_config.mounts.push_back(mnt);
                        }
                }

                if (img_config.contains("linux")) {
                        auto& linux_cfg = img_config["linux"];

                        if (linux_cfg.contains("uidMappings") && linux_cfg["uidMappings"].is_array() && !linux_cfg["uidMappings"].empty()) {
                                auto& m = linux_cfg["uidMappings"][0];
                                if (m.contains("containerID")) container_config.uid_mapping.container_id = m["containerID"].get<uint32_t>();
                                if (m.contains("hostID")) container_config.uid_mapping.host_id = m["hostID"].get<uint32_t>();
                                if (m.contains("size")) container_config.uid_mapping.size = m["size"].get<uint32_t>();
                        }

                        if (linux_cfg.contains("gidMappings") && linux_cfg["gidMappings"].is_array() && !linux_cfg["gidMappings"].empty()) {
                                auto& m = linux_cfg["gidMappings"][0];
                                if (m.contains("containerID")) container_config.gid_mapping.container_id = m["containerID"].get<uint32_t>();
                                if (m.contains("hostID")) container_config.gid_mapping.host_id = m["hostID"].get<uint32_t>();
                                if (m.contains("size")) container_config.gid_mapping.size = m["size"].get<uint32_t>();
                        }

                        if (linux_cfg.contains("maskedPaths") && linux_cfg["maskedPaths"].is_array()) {
                                for (const auto& mp : linux_cfg["maskedPaths"]) {
                                        container_config.masked_paths.paths.push_back(mp.get<std::string>());
                                }
                        }

                        if (linux_cfg.contains("readonlyPaths") && linux_cfg["readonlyPaths"].is_array()) {
                                for (const auto& ro : linux_cfg["readonlyPaths"]) {
                                        container_config.read_only_paths.paths.push_back(ro.get<std::string>());
                                }
                        }
                }

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

        if (!commands.empty()) {
                container_config.args.value = commands;
        }

        container_config.container_id = container_id;
        bool is_terminal{container_config.terminal.value};
        bool is_detached{container_config.detach.value};
        container_config.rootfs.path = outpath + "/rootfs/";

        container_monitor.init(container_config, image_name, container_name, limits, true);
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
                try {
                        Utils::remove_directory(std::format("{}/filesystems/quiver_{}", Utils::get_base_dir().string(), arg));
                }
                catch (const std::exception& e) {
                        std::cerr << e.what() << '\n';
                }
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
                        auto cgroup_manager{CGroupsManagerCreator::create_cgourps_manager(std::to_string(container->pid),
                                        container->config.cgroups_path)};
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
                        auto cgroup_manager{CGroupsManagerCreator::create_cgourps_manager(std::to_string(container->pid),
                                        container->config.cgroups_path)};
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
                ContainerMonitor::Limits limits{};
                container_monitor.init(container->config, container->image, container->name, limits, false);
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
                if (container && container->status != "running" && container->status != "paused") {
                        if (!fs::exists(Utils::get_image_path(container->image) / "config.json")) [[unlikely]] {
                                std::cerr << std::format("Image '{}' doesn't exist please pull image first.\n", container->image);
                                continue;
                        }
                        ContainerMonitor::Limits limits{};
                        limits.cpu_quota = container->cpu_quota;
                        limits.cpu_period = container->cpu_period;
                        limits.cpu_weight = container->cpu_weight;
                        limits.memory_max = container->memory_max;
                        limits.memory_swap = container->memory_max;
                        limits.pids_limit = container->pids_limit;
                        limits.cpuset_cpus = container->cpuset_cpus;
                        limits.cpuset_mems = container->cpuset_mems;
                        limits.io_max_updates = container->io_max_updates;
                        limits.io_weight_updates = container->io_weight_updates;
                        container_monitor.init(container->config, container->image, container->name, limits, false);
                        container_monitor.invoke_container();
                }
                else {
                        std::cerr << std::format("Error: Container '{}' not found or Container already running or paused\n", arg);
                }
        }
}

auto CommandLineHandler::stop(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();

        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }

        auto get_cgroup_path{[](pid_t pid) -> std::optional<fs::path> {
                std::ifstream file(std::format("/proc/{}/cgroup", pid));
                std::string line{};

                while (std::getline(file, line)) {
                        size_t first_colon = line.find(':');
                        if (first_colon != std::string::npos) {
                                size_t second_colon = line.find(':', first_colon + 1);
                                if (second_colon != std::string::npos) {
                                        std::string cg_path{line.substr(second_colon + 1)};
                                        if (!cg_path.empty() && cg_path.front() == '/') {
                                                cg_path.erase(0, 1);
                                        }
                                        return fs::path("/sys/fs/cgroup") / cg_path;
                                }
                        }
                }
                return std::nullopt;
        }};

        for (const auto& arg : args) {
                auto container{container_db_manager.get_container(arg)};
                if (container && (container->status == "running" || container->status == "paused")) {
                        auto cgroup_base_opt{get_cgroup_path(container->pid)};
                        if (cgroup_base_opt) {
                                fs::path cg_kill_path{cgroup_base_opt.value() / "cgroup.kill"};
                                if (fs::exists(cg_kill_path)) {
                                        std::ofstream kill_file(cg_kill_path);
                                        if (kill_file.is_open()) {
                                                kill_file << "1";
                                        }
                                }
                        } else {
                                std::cerr << std::format("WARN: Could not resolve cgroup path for container '{}' via /proc.\n", arg);
                        }

                        if (::kill(container->config.pid, SIGTERM) == 0) {
                                bool stopped{false};
                                for (int i{0}; i < 100; ++i) {
                                        if (!Utils::is_process_alive(container->config.pid, container->config.container_id)) {
                                                stopped = true;
                                                break;
                                        }
                                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                }
                                if (!stopped) {
                                        std::cerr << std::format("WARN: Container '{}' timed out. Sending SIGKILL...\n", arg);
                                        ::kill(container->config.pid, SIGKILL);
                                }
                                for (size_t i{0}; i<50; ++i) {
                                        auto check_container{container_db_manager.get_container(arg)};
                                        if (check_container && check_container->status == "exited") break;
                                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                                }
                                container->status = "stopped";
                                container_db_manager.update_container(arg, container.value());
                                std::cout << std::format("Container '{}' stopped successfully.\n", arg);
                        }
                }
                else {
                        std::cerr << std::format("Error: Container '{}' not found or Container is not running or paused\n", arg);
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
                if (container.status != "running" && container.status != "paused") {
                        container_db_manager.remove_container(container.config.container_id);
                        try {
                                Utils::remove_directory(std::format("{}/filesystems/quiver_{}", Utils::get_base_dir().string(),
                                                        container.config.container_id));
                        }
                        catch (const std::exception& e) {
                                std::cerr << e.what() << '\n';
                        }
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

auto CommandLineHandler::generate_systemd(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();

        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided.\n";
                std::cerr << "Usage: quiver generate-systemd [options] <container_id>\n";
                std::cerr << "Options:\n";
                std::cerr << "  --exe=<path>   Specify a custom path to the quiver executable\n";
                return;
        }

        std::string target_id{};
        std::string custom_bin_path{};

        for (const auto& arg : args) {
                if (arg.starts_with("--exe=")) {
                        custom_bin_path = arg.substr(6);
                } else {
                        target_id = arg;
                }
        }

        if (target_id.empty()) {
                std::cerr << "Error: Container ID is required.\n";
                return;
        }

        auto container{container_db_manager.get_container(target_id)};

        if (!container) {
                std::cerr << std::format("Error: Container '{}' not found in database.\n", target_id);
                return;
        }

        std::string quiver_bin{};
        if (!custom_bin_path.empty()) {
                std::error_code ec{};
                quiver_bin = fs::absolute(custom_bin_path, ec).string();
                if (ec) {
                        quiver_bin = custom_bin_path;
                }
        } else {
                char exe_path[PATH_MAX];
                ssize_t count{readlink("/proc/self/exe", exe_path, sizeof(exe_path))};
                quiver_bin = (count != -1) ? std::string(exe_path, count) : "/usr/local/bin/quiver";
        }

        std::string unit_file = std::format(
                        R"([Unit]
Description=Quiver Container: {0}
Documentation=man:quiver(1)
Wants=network-online.target
After=network-online.target

[Service]
Type=forking
Restart=always
RestartSec=2

# Container Lifecycle Commands
ExecStart={1} start {2}
ExecStop={1} stop {2}

# Optional Host-level Sandboxing for the Quiver CLI invocation
NoNewPrivileges=yes

[Install]
WantedBy=multi-user.target
)",
                container->name,
                quiver_bin,
                container->config.container_id
                        );

        std::cout << unit_file;
}

auto CommandLineHandler::top(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided. Please specify a container ID.\n";
                Utils::print_usage();
                return;
        }
        if (args.size() != 1) {
                std::cerr << "Error: More than one container id provided.\n";
                Utils::print_usage();
                return;
        }
        struct ProcInfo {
                std::string uid{};
                std::string pid{};
                std::string state{};
                std::string cmd{};
        };
        auto target_id{args[0]};
        auto container{container_db_manager.get_container(target_id)};
        if (!container || container->status != "running" || container->status != "paused") {
                std::cerr << std::format("Error: Container '{}' not found or is not running or paused.\n", target_id);
                return;
        }
        auto get_cgroup_path{[](pid_t pid) -> std::optional<std::string> {
                std::ifstream file(std::format("/proc/{}/cgroup", pid));
                std::string line{};
                while (std::getline(file, line)) {
                        size_t first_colon{line.find(':')};
                        if (first_colon != std::string::npos) {
                                size_t second_colon{line.find(':', first_colon + 1)};
                                if (second_colon != std::string::npos) {
                                        return line.substr(second_colon + 1);
                                }
                        }
                }
                return std::nullopt;
        }};
        auto target_cgroup_opt{get_cgroup_path(container->config.pid)};
        if (!target_cgroup_opt) {
                std::cerr << std::format("Error: Could not determine cgroup boundary for container '{}'.\n", target_id);
                return;
        }
        std::string target_cgroup{target_cgroup_opt.value()};
        std::vector<ProcInfo> processes{};
        for (const auto& entry : fs::directory_iterator("/proc")) {
                if (!entry.is_directory()) continue;

                std::string pid_str{entry.path().filename().string()};


                if (!std::all_of(pid_str.begin(), pid_str.end(), ::isdigit)) continue;

                pid_t current_pid{std::stoi(pid_str)};
                auto current_cgroup{get_cgroup_path(current_pid)};
                if (current_cgroup && current_cgroup->starts_with(target_cgroup)) {
                        ProcInfo info{};
                        info.pid = pid_str;

                        struct stat st{};
                        if (stat(entry.path().c_str(), &st) == 0) {
                                passwd* pw{getpwuid(st.st_uid)};
                                info.uid = (pw != nullptr) ? pw->pw_name : std::to_string(st.st_uid);
                        } else {
                                info.uid = "UNKNOWN";
                        }

                        std::ifstream stat_file(entry.path() / "stat");
                        if (stat_file) {
                                std::string dummy_pid{}, comm{}, state{};
                                stat_file >> dummy_pid >> comm >> state;
                                info.state = state;

                                if (comm.size() >= 2 && comm.front() == '(' && comm.back() == ')') {
                                        info.cmd = comm.substr(1, comm.size() - 2);
                                } else {
                                        info.cmd = comm;
                                }
                        }
                        std::ifstream cmd_file(entry.path() / "cmdline");
                        if (cmd_file) {
                                std::ostringstream ss{};
                                ss << cmd_file.rdbuf();
                                std::string full_cmd{ss.str()};

                                if (!full_cmd.empty()) {
                                        for (char& c : full_cmd) {
                                                if (c == '\0') c = ' ';
                                        }
                                        if (full_cmd.back() == ' ') full_cmd.pop_back();
                                        info.cmd = full_cmd;
                                }
                        }
                        processes.emplace_back(info);
                }
        }
        std::cout << std::format("{:<12} {:<10} {:<6} {}\n", "UID", "PID", "STAT", "CMD");
        for (const auto& p : processes) {
                std::cout << std::format("{:<12} {:<10} {:<6} {}\n", p.uid, p.pid, p.state, p.cmd);
        }
}

auto CommandLineHandler::update(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();

        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided.\n";
                std::cerr << "Usage: quiver update [options] <container_id>\n";
                return;
        }

        std::string target_id{};
        int cpu_quota{-1};
        std::uint64_t cpu_period{100000};
        bool update_cpu{false};
        std::uint64_t cpu_weight{0};
        std::uint64_t memory_max{0};
        bool update_memory{false};
        std::uint64_t memory_swap{0};
        bool update_memory_swap{false};
        std::uint64_t pids_limit{0};
        bool update_pids{false};
        std::string cpuset_cpus{};
        std::string cpuset_mems{};
        std::vector<IOMaxUpdate> io_max_updates{};
        std::vector<IOWeightUpdate> io_weight_updates{};

        for (size_t i{0}; i < args.size(); ++i) {
                if (args[i] == "--cpu-quota" && i + 1 < args.size()) {
                        cpu_quota = std::stoi(args[++i]);
                        update_cpu = true;
                } else if (args[i] == "--cpu-period" && i + 1 < args.size()) {
                        cpu_period = std::stoull(args[++i]);
                        update_cpu = true;
                } else if (args[i] == "--cpu-weight" && i + 1 < args.size()) {
                        cpu_weight = std::stoull(args[++i]);
                } else if (args[i] == "--memory-max" && i + 1 < args.size()) {
                        memory_max = std::stoull(args[++i]);
                        update_memory = true;
                } else if (args[i] == "--memory-swap" && i + 1 < args.size()) {
                        memory_swap = std::stoull(args[++i]);
                        update_memory_swap = true;
                } else if (args[i] == "--pids-limit" && i + 1 < args.size()) {
                        pids_limit = std::stoull(args[++i]);
                        update_pids = true;
                } else if (args[i] == "--cpuset-cpus" && i + 1 < args.size()) {
                        cpuset_cpus = args[++i];
                } else if (args[i] == "--set-cpuset-mems" && i + 1 < args.size()) {
                        cpuset_mems = args[++i];
                } else if (args[i] == "--set-io-weight" && i + 1 < args.size()) {
                        auto tokens{split_string(args[++i], ':')};
                        if (tokens.size() == 3) {
                                IOWeightUpdate iw{};
                                iw.major = std::stoull(tokens[0]);
                                iw.minor = std::stoull(tokens[1]);
                                iw.weight = std::stoull(tokens[2]);
                                io_weight_updates.push_back(iw);
                        } else {
                                std::cerr << "Warning: Invalid format for --set-io-weight. Expected MAJOR:MINOR:WEIGHT\n";
                        }
                } else if (args[i] == "--set-io-max" && i + 1 < args.size()) {
                        auto tokens{split_string(args[++i], ':')};
                        if (tokens.size() == 6) {
                                IOMaxUpdate im{};
                                im.major = std::stoull(tokens[0]);
                                im.minor = std::stoull(tokens[1]);
                                im.limits.rbps = std::stoull(tokens[2]);
                                im.limits.wbps = std::stoull(tokens[3]);
                                im.limits.riops = std::stoull(tokens[4]);
                                im.limits.wiops = std::stoull(tokens[5]);
                                io_max_updates.push_back(im);
                        } else {
                                std::cerr << "Warning: Invalid format for --set-io-max. Expected MAJOR:MINOR:RBPS:WBPS:RIOPS:WIOPS\n";
                        }
                } else if (!args[i].starts_with("--")) {
                        if (target_id.empty()) {
                                target_id = args[i];
                        } else {
                                std::cerr << std::format("Warning: Extraneous argument '{}' ignored.\n", args[i]);
                        }
                } else {
                        std::cerr << std::format("Warning: Unknown or incomplete flag '{}'\n", args[i]);
                }
        }

        if (target_id.empty()) {
                std::cerr << "Error: Container ID is required.\n";
                std::cerr << "Usage: quiver update [options] <container_id>\n";
                return;
        }

        auto container{container_db_manager.get_container(target_id)};

        if (!container) {
                std::cerr << std::format("Error: Container '{}' not found.\n", target_id);
                return;
        }

        if (container->status != "running" || container->status != "paused") {
                std::cerr << std::format("Error: Cannot update limits. Container '{}' is not running.\n", target_id);
                return;
        }

        auto cgroups_manager{CGroupsManagerCreator::create_cgourps_manager(
                std::to_string(container->pid), container->config.cgroups_path)};

        try {
                if (update_cpu) {
                        cgroups_manager->set_cpu_limit(cpu_quota, cpu_period);
                        container->cpu_quota = cpu_quota;
                        std::cout << std::format("Updated CPU Quota: {} (Period: {})\n", cpu_quota, cpu_period);
                }
                if (cpu_weight > 0) {
                        cgroups_manager->set_cpu_weight(cpu_weight);
                        container->cpu_weight = cpu_weight;
                        std::cout << std::format("Updated CPU Weight: {}\n", cpu_weight);
                }
                if (update_memory) {
                        cgroups_manager->set_memory_max(memory_max);
                        container->memory_max = memory_max;
                        std::cout << std::format("Updated Memory Max: {} bytes\n", memory_max);
                }
                if (update_memory_swap) {
                        cgroups_manager->set_memory_swap(memory_swap);
                        container->memory_swap = memory_swap;
                        std::cout << std::format("Updated Memory Swap Max: {} bytes\n", memory_swap);
                }
                if (update_pids) {
                        cgroups_manager->set_pid_limit(pids_limit);
                        container->pids_limit = pids_limit;
                        std::cout << std::format("Updated PIDs Limit: {}\n", pids_limit);
                }
                if (!cpuset_cpus.empty()) {
                        cgroups_manager->set_cpuset_cpus(cpuset_cpus);
                        container->cpuset_cpus = cpuset_cpus;
                        std::cout << std::format("Updated CPU Set: {}\n", cpuset_cpus);
                }
                if (!cpuset_mems.empty()) {
                        cgroups_manager->set_cpuset_mems(cpuset_mems);
                        container->cpuset_mems = cpuset_mems;
                        std::cout << std::format("Updated CPU Set Mems: {}\n", cpuset_mems);
                }
                for (const auto& iw : io_weight_updates) {
                        cgroups_manager->set_io_weight(iw.major, iw.minor, iw.weight);
                        std::cout << std::format("Updated IO Weight for device {}:{} to {}\n", iw.major, iw.minor, iw.weight);
                }
                for (const auto& im : io_max_updates) {
                        cgroups_manager->set_io_max(im.major, im.minor, im.limits);
                        std::cout << std::format("Updated IO Max for device {}:{} -> rbps:{} wbps:{} riops:{} wiops:{}\n",
                                                 im.major, im.minor, im.limits.rbps, im.limits.wbps, im.limits.riops, im.limits.wiops);
                }
                container->io_weight_updates = io_weight_updates;
                container->io_max_updates = io_max_updates;
        }
        catch (const std::exception& e) {
                std::cerr << std::format("Error applying updates: {}\n", e.what());
                return;
        }
        container_db_manager.update_container(container->config.container_id, container.value());
        std::cout << std::format("Successfully updated container '{}'.\n", target_id);
}

auto CommandLineHandler::build(std::span<std::string> args) -> void {
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: Build context path not specified.\n";
                std::cerr << "Usage: quiver build [OPTIONS] PATH\n";
                return;
        }

        auto& image_db_manager{ImageDbManager::get_instance()};
        image_db_manager.init();

        std::vector<std::string> tags{};
        std::string quiver_filepath{};
        std::vector<std::string> build_args{};
        std::string target{};
        std::string output_path{};
        bool no_cache{false};
        bool pull{false};
        std::string context_path{};


        for (size_t i{0}; i < args.size(); ++i) {
                const std::string& arg = args[i];
                if (arg == "-t" || arg == "--tag") {
                        if (++i < args.size()) tags.push_back(args[i]);
                } else if (arg == "-f" || arg == "--file") {
                        if (++i < args.size()) quiver_filepath = args[i];
                } else if (arg == "-o" || arg == "--output") {
                        if (++i < args.size()) output_path = args[i];
                } else if (arg == "--build-arg") {
                        if (++i < args.size()) build_args.push_back(args[i]);
                } else if (arg == "--target") {
                        if (++i < args.size()) target = args[i];
                } else if (arg == "--no-cache") {
                        no_cache = true;
                } else if (arg == "--pull") {
                        pull = true;
                } else if (!arg.starts_with("-")) {
                        context_path = arg;
                } else {
                        std::cerr << std::format("Warning: Unknown flag '{}' ignored.\n", arg);
                }
        }

        if (context_path.empty()) [[unlikely]] {
                std::cerr << "Error: Build context path not specified.\n";
                return;
        }

        if (quiver_filepath.empty()) {
                quiver_filepath = (fs::path(context_path) / "Quiverfile").string();
        }

        fs::path df_full_path{fs::absolute(quiver_filepath)};
        fs::path ctx_full_path{fs::absolute(context_path)};

        if (!fs::exists(df_full_path)) [[unlikely]] {
                std::cerr << std::format("Error: Quiverfile not found at '{}'\n", df_full_path.string());
                return;
        }

        ensure_buildkit();
        std::string uid{std::to_string(getuid())};
        std::string socket_path{std::format("/run/user/{}/buildkit/buildkitd.sock", uid)};

        if (!fs::exists(socket_path)) [[unlikely]] {
                std::cerr << "Error: Rootless BuildKit socket failed to initialize.\n";
                return;
        }

        std::vector<std::string> exec_args;
        exec_args.push_back("buildctl");
        exec_args.push_back("--addr");
        exec_args.push_back("unix://" + socket_path);
        exec_args.push_back("build");
        exec_args.push_back("--frontend");
        exec_args.push_back("dockerfile.v0");
        exec_args.push_back("--local");
        exec_args.push_back("context=" + ctx_full_path.string());
        exec_args.push_back("--local");
        exec_args.push_back("dockerfile=" + df_full_path.parent_path().string());
        exec_args.push_back("--opt");
        exec_args.push_back("filename=" + df_full_path.filename().string());

        if (!target.empty()) {
                exec_args.push_back("--opt");
                exec_args.push_back("target=" + target);
        }
        for (const auto& ba : build_args) {
                exec_args.push_back("--opt");
                exec_args.push_back("build-arg:" + ba);
        }
        if (no_cache) exec_args.push_back("--no-cache");
        if (pull) {
                exec_args.push_back("--opt");
                exec_args.push_back("pull=true");
        }


        fs::path final_dest_dir{};
        std::string temp_oci_dir = std::format("/tmp/quiver_oci_{}", getpid());


        if (!tags.empty()) {
                const char* home_dir{std::getenv("HOME")};
                if (home_dir == nullptr) {
                        struct passwd* pw = getpwuid(getuid());
                        if (pw) home_dir = pw->pw_dir;
                }

                std::string primary_tag{tags[0]};
                bool image_tag_found{false};
                size_t idx{0};
                for (char& c : primary_tag) {
                        if (c == ':') {
                                c = '_';
                                image_tag_found = true;
                                idx++;
                                break;
                        }
                }
                if (image_tag_found && idx == primary_tag.size()-1) {
                        primary_tag += "latest";
                        tags[0] += "latest";
                }
                else if (!image_tag_found) {
                        primary_tag += "_latest";
                        tags[0] += ":latest";
                }

                final_dest_dir = fs::path(home_dir) / ".quiver" / "images" / primary_tag / "rootfs";
                if (fs::exists(final_dest_dir.parent_path() / "config.json")) {
                        std::cerr << std::format("Error: Image already exist with '{}'\n", tags[0]);
                        return;
                }
                fs::create_directories(final_dest_dir);


                exec_args.push_back("--output");
                exec_args.push_back("type=local,dest=" + final_dest_dir.string());


                exec_args.push_back("--output");
                exec_args.push_back("type=oci,dest=" + temp_oci_dir + ",tar=false");

                std::cout << std::format("Generating native engine image at: {}\n", final_dest_dir.string());
        }
        else if (!output_path.empty()) {
                fs::path output_tar{fs::absolute(output_path)};
                if (output_tar.has_parent_path()) fs::create_directories(output_tar.parent_path());
                exec_args.push_back("--output");
                exec_args.push_back("type=oci,dest=" + output_tar.string());
                std::cout << std::format("Exporting custom OCI tarball to: {}\n", output_tar.string());
        }

        std::vector<char*> c_args;
        c_args.reserve(exec_args.size() + 1);
        for (auto& arg : exec_args) c_args.push_back(arg.data());
        c_args.push_back(nullptr);

        pid_t pid{fork()};

        if (pid < 0) [[unlikely]] {
                std::cerr << "Error: Failed to fork process for execution.\n";
                return;
        }
        else if (pid == 0) {
                execvp(c_args[0], c_args.data());
                std::cerr << std::format("Error: Failed to execute '{}'. Is buildctl in your PATH?\n", c_args[0]);
                exit(EXIT_FAILURE);
        }
        else {
                int status;
                waitpid(pid, &status, 0);


                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                        if (!final_dest_dir.empty()) {
                                try {
                                        std::ifstream index_file(fs::path(temp_oci_dir) / "index.json");
                                        nlohmann::json index_json;
                                        index_file >> index_json;

                                        std::string manifest_digest = index_json["manifests"][0]["digest"];
                                        std::string manifest_hash = manifest_digest.substr(7);


                                        std::ifstream manifest_file(fs::path(temp_oci_dir) / "blobs" / "sha256" / manifest_hash);
                                        nlohmann::json manifest_json;
                                        manifest_file >> manifest_json;

                                        std::string config_digest = manifest_json["config"]["digest"];
                                        std::string config_hash = config_digest.substr(7);

                                        fs::path source_config{fs::path(temp_oci_dir) / "blobs" / "sha256" / config_hash};
                                        fs::path dest_config{final_dest_dir.parent_path() / "config.json"};
                                        std::error_code ec;
                                        if (fs::exists(dest_config)) {
                                                fs::permissions(dest_config, fs::perms::owner_write, fs::perm_options::add, ec);
                                                fs::remove(dest_config, ec);
                                        }

                                        fs::copy_file(source_config, dest_config, fs::copy_options::overwrite_existing);

                                        fs::path image_dir{Utils::get_image_path(tags[0])};
                                        auto index{tags[0].find(':')};
                                        ImageMetadata image_metadata{};
                                        image_metadata.id = Utils::generate_id();
                                        image_metadata.name = tags[0].substr(0, index);
                                        image_metadata.tag = tags[0].substr(index+1);
                                        image_metadata.size_bytes = 0;

                                        try {
                                                auto dir_opts{fs::directory_options::skip_permission_denied};
                                                std::set<std::pair<dev_t, ino_t>> seen_inodes{};

                                                for (const auto& entry : fs::recursive_directory_iterator(image_dir, dir_opts)) {
                                                        if (entry.is_regular_file() && !entry.is_symlink()) {
                                                                struct stat st;
                                                                if (stat(entry.path().c_str(), &st) == 0) {
                                                                        if (seen_inodes.insert({st.st_dev, st.st_ino}).second) {
                                                                                image_metadata.size_bytes += st.st_size;
                                                                        }
                                                                }
                                                        }
                                                }
                                        } catch (const std::exception& e) {
                                                std::cerr << "Warning: Could not accurately calculate total image size: " << e.what() << "\n";
                                        }

                                        image_metadata.source = fs::absolute(quiver_filepath);
                                        image_db_manager.add_image(image_metadata);
                                        std::cout << "Successfully embedded config.json.\n";
                                        std::cout << "Build completed successfully!\n";
                                } catch (const std::exception& e) {
                                        std::cerr << "Warning: Rootfs built, but failed to process config metadata: " << e.what() << "\n";
                                }
                        } else {
                                std::cout << "[Quiver Build] Build completed successfully!\n";
                        }
                } else if (WIFSIGNALED(status)) {
                        std::cerr << std::format("Error: Process killed by signal {}\n", WTERMSIG(status));
                } else {
                        std::cerr << std::format("Error: Build failed with exit code {}\n", WEXITSTATUS(status));
                }

                std::error_code ec{};
                if (!temp_oci_dir.empty() && fs::exists(temp_oci_dir)) {
                        fs::remove_all(temp_oci_dir, ec);
                }
        }
}

auto CommandLineHandler::create(std::span<std::string> args) -> void {
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided.\n";
                std::cerr << "Usage: quiver create [OPTIONS] IMAGE:TAG [COMMANDS...]\n";
                return;
        }

        std::string json_path{};
        std::string image_name{};
        std::vector<std::string> custom_cmds{};
        for (size_t i{0}; i < args.size(); ++i) {
                const std::string& arg = args[i];

                if (arg == "--from-json" || arg == "-j") {
                        if (++i < args.size()) {
                                json_path = args[i];
                        } else {
                                std::cerr << std::format("Error: {} requires a file path argument.\n", arg);
                                return;
                        }
                }
                else if (!arg.starts_with("-") && image_name.empty()) {
                        image_name = arg;
                }
                else if (!image_name.empty()) {
                        custom_cmds.push_back(arg);
                }
                else {
                        std::cerr << std::format("Warning: Unknown flag '{}' ignored.\n", arg);
                }
        }

        std::string container_id = std::format("qvr-{:x}", std::time(nullptr));

        ContainerConfig config{};
        if (!json_path.empty()) {
                try {
                        config = ConfigParser::parse_file(fs::absolute(json_path));
                        if (config.container_id.empty()) {
                                config.container_id = container_id;
                        } else {
                                container_id = config.container_id;
                        }

                        if (!image_name.empty()) {
                                config.rootfs.path = Utils::get_image_path(image_name);
                                config.rootfs.read_only = false;
                        }
                } catch (const std::exception& e) {
                        std::cerr << "Error loading JSON config: " << e.what() << "\n";
                        return;
                }
        } else {
                if (image_name.empty()) {
                        std::cerr << "Error: Image name is required when not using a complete JSON configuration.\n";
                        std::cerr << "Usage: quiver create [OPTIONS] IMAGE:TAG [COMMANDS...]\n";
                        return;
                }
                config = SpecGenerator::generate_default_rootless_spec(container_id, Utils::get_image_path(image_name));
        }

        if (!custom_cmds.empty()) {
                config.args.value = custom_cmds;
        }
        auto& container_db_manager{ContainerDbManager::get_instance()};
        ContainerDbObject db_object{};
        db_object.config = config;
        db_object.image = image_name;
        db_object.name = std::format("quiver_{}", config.container_id.substr(0, 6));
        db_object.status = "created";
        db_object.boot_time = Utils::get_boot_time();
        db_object.created_at = std::format("{}", std::chrono::system_clock::now());
        container_db_manager.add_container(db_object);
}

auto CommandLineHandler::image(std::span<std::string> args) -> void {
        auto& image_db_manager{ImageDbManager::get_instance()};
        image_db_manager.init();
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided.\n";
                std::cerr << "Usage: quiver create [OPTIONS] IMAGE:TAG [COMMANDS...]\n";
                return;
        }
        if (args.front() == "ls") {
                if (args.size() != 1) {
                        std::cerr << "Error: ls only lists images no arguments needed\n";
                        return;
                }
                auto images{image_db_manager.get_all_images()};
                std::cout << std::format("{:<70} {:<25} {:<15} {:<15} {:<20}\n",
                                         "IMAGE ID", "NAME", "TAG", "SIZE", "SOURCE");
                for (const auto& image : images) {
                        double size_mb{static_cast<double>(image.size_bytes) / (1024.0 * 1024.0)};
                        std::string size_str{std::format("{:.2f} MB", size_mb)};
                        std::cout << std::format("{:<70} {:<25} {:<15} {:<15} {:<20}\n", image.id, image.name, image.tag,
                                        size_str, image.source);
                }
        }
        else if (args.front() == "rm") {
                if (args.size() < 2) [[unlikely]] {
                        std::cerr << "Error: No image specified for removal.\n";
                        std::cerr << "Usage: quiver rm IMAGE [IMAGE...]\n";
                        return;
                }
                for (size_t i{1}; i < args.size(); ++i) {
                        const std::string& target{args[i]};
                        auto image{image_db_manager.get_image(target)};

                        if (image) {
                                auto& container_db_manager{ContainerDbManager::get_instance()};
                                container_db_manager.init();
                                auto containers{container_db_manager.get_all_container()};
                                bool image_in_use{false};
                                std::string image_full_name{std::format("{}:{}", image->name, image->tag)};
                                for (const auto& container : containers) {
                                        if (container.status == "running" || container.status == "paused") {
                                                if (container.image == image_full_name ||
                                                    container.image == image->name ||
                                                    container.image == image->id ||
                                                    container.image == target) {
                                                        image_in_use = true;
                                                        break;
                                                }
                                        }
                                }

                                if (image_in_use) {
                                        std::cerr << std::format("Error: Image '{}' is in use by a running or paused container and cannot be deleted.\n", target);
                                        continue;
                                }

                                fs::path dir{Utils::get_image_path(std::format("{}_{}", image->name, image->tag))};
                                try {
                                        if (fs::exists(dir)) {
                                                Utils::remove_directory(dir);
                                        }
                                        image_db_manager.remove_image(target);
                                        std::cout << "Deleted: " << target << '\n';
                                }
                                catch (const std::exception& e) {
                                        std::cerr << std::format("Error: Failed to remove image '{}': {}\n", target, e.what());
                                }
                        } else {
                                std::cerr << std::format("Error: No such image: {}\n", target);
                        }
                }
        }
        else if (args.front() == "load") {
                if (args.size() != 3) {
                        std::cerr << "Usage: quiver load IMAGE_NAME[:TAG] TAR_PATH\n";
                        Utils::print_usage();
                        return;
                }

                std::string image_name{};
                std::string image_tag{};
                auto index{args[1].find(':')};

                if (index != std::string::npos) {
                        image_name = args[1].substr(0, index);
                        image_tag = args[1].substr(index + 1);
                } else {
                        image_name = args[1];
                        image_tag = "latest";
                }

                if (image_tag.empty()) image_tag = "latest";

                auto image_dir{Utils::get_image_path(std::format("{}_{}", image_name, image_tag))};
                fs::path tar_path{args[2]};

                if (!Utils::load_oci_tar(tar_path, image_dir)) [[unlikely]] {
                        std::cerr << "Error: Unable to load tar file into image directory.\n";
                        return;
                }

                ImageMetadata image_metadata{};
                image_metadata.id = Utils::generate_id();
                image_metadata.name = image_name;
                image_metadata.tag = image_tag;
                image_metadata.size_bytes = 0;

                try {
                        auto dir_opts = fs::directory_options::skip_permission_denied;
                        std::set<std::pair<dev_t, ino_t>> seen_inodes{};

                        for (const auto& entry : fs::recursive_directory_iterator(image_dir, dir_opts)) {
                                if (entry.is_regular_file() && !entry.is_symlink()) {
                                        struct stat st;
                                        if (stat(entry.path().c_str(), &st) == 0) {
                                                if (seen_inodes.insert({st.st_dev, st.st_ino}).second) {
                                                        image_metadata.size_bytes += st.st_size;
                                                }
                                        }
                                }
                        }
                } catch (const std::exception& e) {
                        std::cerr << "Warning: Could not accurately calculate total image size: " << e.what() << "\n";
                }

                image_metadata.source = tar_path.filename().string();
                image_db_manager.add_image(image_metadata);
                double size_mb = static_cast<double>(image_metadata.size_bytes) / (1024.0 * 1024.0);
                std::cout << std::format("Successfully loaded image {}:{} ({:.2f} MB)\n",
                                         image_metadata.name, image_metadata.tag, size_mb);
        }
        else if (args.front() == "pull") {
                if (args.size() < 2) {
                        std::cerr << "Error: No image specified.\n";
                        Utils::print_usage();
                        return;
                }
                auto& image_manager{ImageManager::get_instance()};
                image_manager.init();
                for (size_t i{1}; i<args.size(); ++i) {
                        fs::path image_dir{Utils::get_image_path(args[i])};
                        if (fs::exists(image_dir / "config.json")) {
                                std::cout << std::format("Image '{}' found locally\n", args[i]);
                                continue;
                        }
                        std::string out_path{};
                        std::string error{};
                        image_manager.pull(args[i], out_path, error);
                        if (!error.empty()) [[unlikely]] {
                                std::cerr << error << '\n';
                                continue;
                        }
                        std::string image_name{};
                        std::string image_tag{};
                        auto index{args[i].find(':')};
                        if (index != std::string::npos) {
                                image_name = args[i].substr(0, index);
                                image_tag = args[i].substr(index + 1);
                        } else {
                                image_name = args[i];
                                image_tag = "latest";
                        }
                        ImageMetadata image_metadata{};
                        image_metadata.id = Utils::generate_id();
                        image_metadata.name = image_name;
                        image_metadata.tag = image_tag;
                        image_metadata.size_bytes = 0;

                        try {
                                auto dir_opts{fs::directory_options::skip_permission_denied};
                                std::set<std::pair<dev_t, ino_t>> seen_inodes{};

                                for (const auto& entry : fs::recursive_directory_iterator(image_dir, dir_opts)) {
                                        if (entry.is_regular_file() && !entry.is_symlink()) {
                                                struct stat st;
                                                if (stat(entry.path().c_str(), &st) == 0) {
                                                        if (seen_inodes.insert({st.st_dev, st.st_ino}).second) {
                                                                image_metadata.size_bytes += st.st_size;
                                                        }
                                                }
                                        }
                                }
                        } catch (const std::exception& e) {
                                std::cerr << "Warning: Could not accurately calculate total image size: " << e.what() << "\n";
                        }

                        image_metadata.source = "dockerhub";
                        image_db_manager.add_image(image_metadata);
                        std::cout << std::format("Successfully pulled Image '{}'\n", args[i]);
                }
        }
        else {
                if (args.size() != 1) {
                        std::cerr << "Error: No or more than one image id specified.\n";
                        Utils::print_usage();
                        return;
                }
                auto image{image_db_manager.get_image(args[0])};
                if (image) {
                        double size_mb{static_cast<double>(image->size_bytes) / (1024.0 * 1024.0)};
                        std::string size_str{std::format("{:.2f} MB", size_mb)};
                        std::cout << std::format("{:<70} {:<25} {:<15} {:<15} {:<20}\n",
                                        "IMAGE ID", "NAME", "TAG", "SIZE", "SOURCE");
                        std::cout << std::format("{:<70} {:<25} {:<15} {:<15} {:<20}\n", image->id, image->name, image->tag,
                                        size_str, image->source);
                }
                else {
                        std::cerr << std::format("Error: No such image: {}\n", args[0]);
                }
        }
}

auto CommandLineHandler::restart(std::span<std::string> args) -> void {
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
        auto get_cgroup_path{[](pid_t pid) -> std::optional<fs::path> {
                std::ifstream file(std::format("/proc/{}/cgroup", pid));
                std::string line{};

                while (std::getline(file, line)) {
                        size_t first_colon = line.find(':');
                        if (first_colon != std::string::npos) {
                                size_t second_colon = line.find(':', first_colon + 1);
                                if (second_colon != std::string::npos) {
                                        std::string cg_path{line.substr(second_colon + 1)};
                                        if (!cg_path.empty() && cg_path.front() == '/') {
                                                cg_path.erase(0, 1);
                                        }
                                        return fs::path("/sys/fs/cgroup") / cg_path;
                                }
                        }
                }
                return std::nullopt;
        }};
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        auto& container_monitor{ContainerMonitor::get_instance()};
        auto container{container_db_manager.get_container(args[0])};
        if (container && container->status == "running") {
                auto cgroup_base_opt{get_cgroup_path(container->pid)};
                if (cgroup_base_opt) {
                        fs::path cg_kill_path = cgroup_base_opt.value() / "cgroup.kill";
                        if (fs::exists(cg_kill_path)) {
                                std::ofstream kill_file(cg_kill_path);
                                if (kill_file.is_open()) {
                                        kill_file << "1";
                                }
                        }
                } else {
                        std::cerr << std::format("WARN: Could not resolve cgroup path for container '{}' via /proc.\n", args[0]);
                }
                if (::kill(container->config.pid, SIGTERM) == 0) {
                        bool stopped{false};
                        for (int i{0}; i < 100; ++i) {
                                if (!Utils::is_process_alive(container->config.pid, container->config.container_id)) {
                                        stopped = true;
                                        break;
                                }
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                        if (!stopped) {
                                std::cerr << std::format("WARN: Container '{}' timed out. Sending SIGKILL...\n", args[0]);
                                ::kill(container->config.pid, SIGKILL);
                        }
                        for (int i{0}; i < 50; ++i) {
                                auto check_container{container_db_manager.get_container(args[0])};
                                if (check_container && check_container->status == "exited") {
                                        break;
                                }
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                }
                auto fresh_container{container_db_manager.get_container(args[0])};
                if (!fresh_container) {
                        std::cerr << "Error: Container lost during restart.\n";
                        return;
                }
                ContainerMonitor::Limits limits{};
                limits.cpu_quota = fresh_container->cpu_quota;
                limits.cpu_period = fresh_container->cpu_period;
                limits.cpu_weight = fresh_container->cpu_weight;
                limits.memory_max = fresh_container->memory_max;
                limits.memory_swap = fresh_container->memory_max;
                limits.pids_limit = fresh_container->pids_limit;
                limits.cpuset_cpus = fresh_container->cpuset_cpus;
                limits.cpuset_mems = fresh_container->cpuset_mems;
                limits.io_max_updates = fresh_container->io_max_updates;
                limits.io_weight_updates = fresh_container->io_weight_updates;

                container_monitor.init(fresh_container->config, fresh_container->image, fresh_container->name, limits, false);
                container_monitor.invoke_container();
        }
        else {
                std::cerr << std::format("Error: Container '{}' not found or Container is not running\n", args[0]);
        }
}

auto CommandLineHandler::mount(std::span<std::string> args) -> void {
        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided\n";
                Utils::print_usage();
                return;
        }
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        if (args.front() == "ls") {
                if (args.size() != 1) {
                        std::cerr << "Error: ls only lists mounts no arguments needed\n";
                }
                auto containers{container_db_manager.get_all_container()};
                constexpr int id_width{70};
                constexpr int src_width{45};
                constexpr int dest_width{45};
                constexpr int type_width{15};

                std::cout << std::format("{:<{}} {:<{}} {:<{}} {:<{}}\n",
                                "CONTAINER ID", id_width,
                                "SOURCE", src_width,
                                "DESTINATION", dest_width,
                                "TYPE", type_width);

                for (const auto& container : containers) {
                        for (const auto& mount : container.config.mounts) {
                                std::cout << std::format("{:<{}} {:<{}} {:<{}} {:<{}}\n",
                                                container.config.container_id, id_width,
                                                mount.source, src_width,
                                                mount.destination, dest_width,
                                                mount.type, type_width);
                        }
                }
        }
        else if (args.front() == "add") {
                if (args.size() < 3) {
                        std::cerr << "Error: Not enough arguments provided.\n";
                        std::cerr << "Usage: quiver mount add <container_id> <host_path>:<container_path> [...]\n";
                        return;
                }

                std::string target_id = args[1];

                auto& container_db_manager{ContainerDbManager::get_instance()};
                container_db_manager.init();

                auto container{container_db_manager.get_container(target_id)};
                if (!container) {
                        std::cerr << std::format("Error: Container '{}' not found.\n", target_id);
                        return;
                }

                if (container->status == "running" || container->status == "paused") {
                        std::cerr << std::format("Warning: Container '{}' is running or paused. Volume changes will take effect on next restart.\n", target_id);
                }

                for (size_t i{2}; i < args.size(); ++i) {
                        auto tokens = split_string(args[i], ':');
                        if (tokens.size() < 2) {
                                std::cerr << std::format("Warning: Invalid mount format '{}'. Expected <host_path>:<container_path>\n", args[i]);
                                continue;
                        }

                        OCIRuntime::Mount mnt{};
                        mnt.source = tokens[0];
                        mnt.destination = tokens[1];
                        mnt.type = "bind";
                        mnt.options.push_back("rbind");

                        if (tokens.size() >= 3) {
                                if (tokens[2] == "ro") {
                                        mnt.options.push_back("ro");
                                } else if (tokens[2] == "rw") {
                                        mnt.options.push_back("rw");
                                }
                        }
                        container->config.mounts.emplace_back(mnt);
                        std::cout << std::format("Added mount mapping {} -> {} to container '{}'\n",
                                        tokens[0], tokens[1], target_id);
                }

                container_db_manager.update_container(target_id, container.value());
                std::cout << "Successfully updated mount configurations.\n";
        }
        else if (args.front() == "rm") {
                if (args.size() < 3) {
                        std::cerr << "Error: Not enough arguments provided.\n";
                        std::cerr << "Usage: quiver mount rm <container_id> <container_path> [...]\n";
                        return;
                }

                std::string target_id{args[1]};

                auto& container_db_manager{ContainerDbManager::get_instance()};
                container_db_manager.init();

                auto container{container_db_manager.get_container(target_id)};
                if (!container) {
                        std::cerr << std::format("Error: Container '{}' not found.\n", target_id);
                        return;
                }
                if (container->status == "running" || container->status == "paused") {
                        std::cerr << std::format("Warning: Container '{}' is running or paused. Volume changes will take effect on next restart.\n", target_id);
                }
                bool modified{false};

                for (size_t i{2}; i < args.size(); ++i) {
                        std::string target_dest = args[i];
                        auto& mounts{container->config.mounts};

                        auto it{std::remove_if(mounts.begin(), mounts.end(),
                                [&target_dest](const OCIRuntime::Mount& mnt) {
                                        return mnt.destination == target_dest;
                                })};

                        if (it != mounts.end()) {
                                mounts.erase(it, mounts.end());
                                std::cout << std::format("Removed mount mapping for '{}' from container '{}'\n",
                                                         target_dest, target_id);
                                modified = true;
                        } else {
                                std::cerr << std::format("Warning: No mount mounted at '{}' found in container '{}'\n",
                                                         target_dest, target_id);
                        }
                }

                if (modified) {
                        container_db_manager.update_container(target_id, container.value());
                        std::cout << "Successfully updated mount configurations.\n";
                }
        }
        else {
                std::cerr << std::format("quiver mount: '{}' is not a quiver mount command.\n", args.front());
                Utils::print_usage();
        }
}

auto CommandLineHandler::exec(std::span<std::string> args) -> void {
        bool allocate_pty{false};
        std::string container_id;
        std::vector<std::string> cmd_args;

        for (const auto& arg : args) {
                if (arg == "-t" || arg == "-it" || arg == "-i") {
                        allocate_pty = true;
                } else if (container_id.empty()) {
                        container_id = arg;
                } else {
                        cmd_args.push_back(arg);
                }
        }

        if (container_id.empty() || cmd_args.empty()) {
                std::cerr << "Usage: quiver exec [-t|-it] <container_id> <command> [args...]\n";
                return;
        }

        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();
        auto container{container_db_manager.get_container(container_id)};

        if (!container || container->status != "running") {
                std::cerr << "Error: Container not found or not running.\n";
                return;
        }

        pid_t target_pid = container->config.pid;

        int master_fd{-1}, slave_fd{-1};
        if (allocate_pty) {
                struct winsize ws{};
                if (isatty(STDIN_FILENO)) {
                        if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
                                std::cerr << "Warning: Failed to get host terminal size\n";
                                ws.ws_row = 24;
                                ws.ws_col = 80;
                        }
                } else {
                        ws.ws_row = 24;
                        ws.ws_col = 80;
                }
                if (openpty(&master_fd, &slave_fd, nullptr, nullptr, &ws) == -1) {
                        std::cerr << "Error: Failed to allocate PTY\n";
                        return;
                }
        }

        pid_t worker_pid = fork();
        if (worker_pid < 0) [[unlikely]] {
                std::cerr << "Error: Failed to fork worker process\n";
                return;
        }

        if (worker_pid == 0) {
                if (allocate_pty) {
                        close(master_fd);
                }

                std::string target_root = std::format("/proc/{}/root", target_pid);
                int root_fd = open(target_root.c_str(), O_RDONLY | O_DIRECTORY);

                const std::vector<std::string> namespaces = {"user", "ipc", "uts", "net", "pid", "mnt"};
                for (const auto& ns : namespaces) {
                        std::string ns_path = std::format("/proc/{}/ns/{}", target_pid, ns);
                        int fd = open(ns_path.c_str(), O_RDONLY | O_CLOEXEC);

                        if (fd >= 0) {
                                if (setns(fd, 0) == -1) {
                                        std::cerr << std::format("Error: Failed to join {} namespace. Are you the owner of this container?\n", ns);
                                        _exit(EXIT_FAILURE);
                                }
                                close(fd);
                        } else {
                                std::cerr << std::format("Error: Cannot open {} namespace. Check permissions.\n", ns);
                                _exit(EXIT_FAILURE);
                        }
                }

                if (root_fd >= 0) {
                        fchdir(root_fd);
                        chroot(".");
                        close(root_fd);
                }

                setuid(0);
                setgid(0);

                pid_t exec_pid{fork()};
                if (exec_pid == 0) {
                        if (allocate_pty) {
                                setsid();
                                if (ioctl(slave_fd, TIOCSCTTY, 0) == -1) {
                                        std::cerr << "Warning: Failed to set controlling terminal\n";
                                }

                                dup2(slave_fd, STDIN_FILENO);
                                dup2(slave_fd, STDOUT_FILENO);
                                dup2(slave_fd, STDERR_FILENO);
                                close(slave_fd);
                        } else {
                                int null_fd = open("/dev/null", O_RDONLY);
                                if (null_fd >= 0) {
                                        dup2(null_fd, STDIN_FILENO);
                                        close(null_fd);
                                }
                        }

                        std::vector<char*> c_args;
                        c_args.reserve(cmd_args.size() + 1);
                        for (auto& str : cmd_args) {
                                c_args.push_back(str.data());
                        }
                        c_args.push_back(nullptr);

                        if (clearenv() != 0) [[unlikely]] {
                                std::cerr << "Error: clearenv failed in exec.\n";
                                _exit(EXIT_FAILURE);
                        }

                        for (const auto& env : container->config.env.value) {
                                auto it{env.find('=')};
                                if (it != std::string::npos && it > 0) {
                                        std::string name{env.substr(0, it)};
                                        std::string value{env.substr(it+1)};
                                        if (setenv(name.c_str(), value.c_str(), 1) != 0) [[unlikely]] {
                                                std::cerr << std::format("Warning: setenv failed for -> {}\n", env);
                                        }
                                }
                                else [[unlikely]] {
                                        std::cerr << std::format("Warning: Invalid env format ignored -> {}\n", env);
                                }
                        }

                        setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 0);
                        if (allocate_pty) {
                                setenv("TERM", "xterm-256color", 1);
                        }

                        execvp(c_args[0], c_args.data());
                        std::cerr << std::format("exec failed: {}\n", strerror(errno));
                        _exit(EXIT_FAILURE);
                }

                if (allocate_pty) close(slave_fd);
                int status{};
                waitpid(exec_pid, &status, 0);
                _exit(WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
        }

        if (allocate_pty) {
                close(slave_fd);

                struct termios orig_termios{}, raw_termios{};
                tcgetattr(STDIN_FILENO, &orig_termios);
                raw_termios = orig_termios;
                cfmakeraw(&raw_termios);
                tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);

                struct pollfd fds[2];
                fds[0].fd = STDIN_FILENO;
                fds[0].events = POLLIN;
                fds[1].fd = master_fd;
                fds[1].events = POLLIN;

                char buf[8192];
                while (true) {
                        if (poll(fds, 2, -1) <= 0) break;

                        if (fds[0].revents & POLLIN) {
                                ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
                                if (n > 0) {
                                        write(master_fd, buf, n);
                                } else {
                                        break;
                                }
                        }

                        if (fds[0].revents & (POLLHUP | POLLERR)) {
                                break;
                        }

                        if (fds[1].revents & POLLIN) {
                                ssize_t n = read(master_fd, buf, sizeof(buf));
                                if (n > 0) {
                                        write(STDOUT_FILENO, buf, n);
                                } else {
                                        break;
                                }
                        }

                        if (fds[1].revents & (POLLHUP | POLLERR)) {
                                break;
                        }
                }

                tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
                close(master_fd);
        }
        int status{};
        waitpid(worker_pid, &status, 0);
}

auto CommandLineHandler::wait(std::span<std::string> args) -> void {
        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();

        if (args.empty()) [[unlikely]] {
                std::cerr << "Error: No arguments provided. Please specify at least one container ID.\n";
                Utils::print_usage();
                return;
        }

        for (const auto& arg : args) {
                while (true) {
                        auto container{container_db_manager.get_container(arg)};
                        if (!container) {
                                std::cerr << std::format("Error: Container '{}' not found.\n", arg);
                                break;
                        }

                        if (container->status == "exited" || container->status == "stopped") {
                                break;
                        }

                        if (container->status == "running" && container->config.pid > 0) {
                                if (!Utils::is_process_alive(container->config.pid, container->config.container_id)) {
                                        container->status = "exited";
                                        container->exit_code = 137;
                                        container_db_manager.update_container(arg, container.value());
                                        break;
                                }
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
        }
}

auto CommandLineHandler::kill(std::span<std::string> args) -> void {
        auto parse_signal{[](std::string sig_str) -> int {
                std::transform(sig_str.begin(), sig_str.end(), sig_str.begin(), ::toupper);


                if (sig_str.starts_with("SIG")) {
                        sig_str = sig_str.substr(3);
                }

                const std::pair<std::string, int> sig_map[] = {
                        {"HUP", 1}, {"INT", 2}, {"QUIT", 3}, {"ILL", 4}, {"TRAP", 5},
                        {"ABRT", 6}, {"BUS", 7}, {"FPE", 8}, {"KILL", 9}, {"USR1", 10},
                        {"SEGV", 11}, {"USR2", 12}, {"PIPE", 13}, {"ALRM", 14}, {"TERM", 15},
                        {"CHLD", 17}, {"CONT", 18}, {"STOP", 19}, {"TSTP", 20}, {"TTIN", 21},
                        {"TTOU", 22}
                };

                for (const auto& [name, num] : sig_map) {
                        if (sig_str == name) return num;
                }


                try {
                        int sig{std::stoi(sig_str)};
                        if (sig > 0 && sig <= 64) return sig;
                } catch (...) {}

                return -1;
        }};

        int signal_to_send{9};
        std::vector<std::string> target_containers{};

        for (size_t i{0}; i < args.size(); ++i) {
                if (args[i] == "-s" || args[i] == "--signal") {
                        if (++i < args.size()) {
                                signal_to_send = parse_signal(args[i]);
                                if (signal_to_send == -1) [[unlikely]] {
                                        std::cerr << std::format("Error: Invalid signal '{}' specified.\n", args[i]);
                                        return;
                                }
                        } else [[unlikely]] {
                                std::cerr << std::format("Error: Signal not provided after '{}'.\n", args[i - 1]);
                                return;
                        }
                } else if (!args[i].starts_with("-")) {
                        target_containers.push_back(args[i]);
                } else {
                        std::cerr << std::format("Warning: Unknown flag '{}' ignored.\n", args[i]);
                }
        }

        if (target_containers.empty()) [[unlikely]] {
                std::cerr << "Error: No container ID specified.\n";
                Utils::print_usage();
                return;
        }

        auto& container_db_manager{ContainerDbManager::get_instance()};
        container_db_manager.init();

        for (const auto& arg : target_containers) {
                auto container{container_db_manager.get_container(arg)};

                if (!container) {
                        std::cerr << std::format("Error: Container '{}' not found.\n", arg);
                        continue;
                }

                if (container->status != "running" && container->status != "paused") {
                        std::cerr << std::format("Error: Container '{}' is not running.\n", arg);
                        continue;
                }

                if (container->config.pid <= 0) [[unlikely]] {
                        std::cerr << std::format("Error: Invalid PID ({}) in database. Marking container '{}' as exited.\n",
                                                 container->config.pid, arg);
                        container->status = "exited";
                        container_db_manager.update_container(arg, container.value());
                        continue;
                }

                if (::kill(container->config.pid, signal_to_send) == 0) {
                        std::cout << arg << '\n';
                } else {
                        std::cerr << std::format("Error: Failed to send signal to container '{}' -> {}\n",
                                                 arg, std::strerror(errno));
                }
        }
}
