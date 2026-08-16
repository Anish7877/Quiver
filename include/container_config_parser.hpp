#pragma once
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include "container_config.hpp"

namespace OCIRuntime {

        inline void from_json(const nlohmann::json& j, Root& obj) {
                if (j.contains("path")) obj.path = j.at("path").get<std::string>();
                if (j.contains("read_only")) j.at("read_only").get_to(obj.read_only);
        }

        inline void from_json(const nlohmann::json& j, Terminal& obj) {
                if (j.contains("value")) j.at("value").get_to(obj.value);
        }

        inline void from_json(const nlohmann::json& j, Detach& obj) {
                if (j.contains("value")) j.at("value").get_to(obj.value);
        }

        inline void from_json(const nlohmann::json& j, ConsoleSize& obj) {
                if (j.contains("height")) j.at("height").get_to(obj.height);
                if (j.contains("width")) j.at("width").get_to(obj.width);
        }

        inline void from_json(const nlohmann::json& j, User& obj) {
                if (j.contains("uid")) j.at("uid").get_to(obj.uid);
                if (j.contains("gid")) j.at("gid").get_to(obj.gid);
                if (j.contains("umask")) j.at("umask").get_to(obj.umask);
                if (j.contains("additional_gids")) j.at("additional_gids").get_to(obj.additional_gids);
        }

        inline void from_json(const nlohmann::json& j, UidMapping& obj) {
                if (j.contains("container_id")) j.at("container_id").get_to(obj.container_id);
                if (j.contains("host_id")) j.at("host_id").get_to(obj.host_id);
                if (j.contains("size")) j.at("size").get_to(obj.size);
        }

        inline void from_json(const nlohmann::json& j, GidMapping& obj) {
                if (j.contains("container_id")) j.at("container_id").get_to(obj.container_id);
                if (j.contains("host_id")) j.at("host_id").get_to(obj.host_id);
                if (j.contains("size")) j.at("size").get_to(obj.size);
        }

        inline void from_json(const nlohmann::json& j, Env& obj) {
                if (j.contains("value")) j.at("value").get_to(obj.value);
        }

        inline void from_json(const nlohmann::json& j, Cwd& obj) {
                if (j.contains("value")) j.at("value").get_to(obj.value);
        }

        inline void from_json(const nlohmann::json& j, Args& obj) {
                if (j.contains("value")) j.at("value").get_to(obj.value);
        }

        inline void from_json(const nlohmann::json& j, OomScoreAdj& obj) {
                if (j.contains("value")) j.at("value").get_to(obj.value);
        }

        inline void from_json(const nlohmann::json& j, SchedularOpts& obj) {
                if (j.contains("flags")) j.at("flags").get_to(obj.flags);
                if (j.contains("policy")) j.at("policy").get_to(obj.policy);
                if (j.contains("runtime")) j.at("runtime").get_to(obj.runtime);
                if (j.contains("deadline")) j.at("deadline").get_to(obj.deadline);
                if (j.contains("period")) j.at("period").get_to(obj.period);
                if (j.contains("nice")) j.at("nice").get_to(obj.nice);
                if (j.contains("priority")) j.at("priority").get_to(obj.priority);
        }

        inline void from_json(const nlohmann::json& j, NoNewPrivileges& obj) {
                if (j.contains("value")) j.at("value").get_to(obj.value);
        }

        inline void from_json(const nlohmann::json& j, Capabilities& obj) {
                if (j.contains("bounding")) j.at("bounding").get_to(obj.bounding);
                if (j.contains("effective")) j.at("effective").get_to(obj.effective);
                if (j.contains("inheritable")) j.at("inheritable").get_to(obj.inheritable);
                if (j.contains("permitted")) j.at("permitted").get_to(obj.permitted);
                if (j.contains("ambient")) j.at("ambient").get_to(obj.ambient);
        }

        inline void from_json(const nlohmann::json& j, RLimit& obj) {
                if (j.contains("name")) j.at("name").get_to(obj.name);
                if (j.contains("hard_limit")) j.at("hard_limit").get_to(obj.hard_limit);
                if (j.contains("soft_limit")) j.at("soft_limit").get_to(obj.soft_limit);
        }

        inline void from_json(const nlohmann::json& j, RootfsPropagation& obj) {
                if (j.contains("type")) j.at("type").get_to(obj.type);
        }

        inline void from_json(const nlohmann::json& j, Seccomp::Arg& obj) {
                if (j.contains("op")) j.at("op").get_to(obj.op);
                if (j.contains("value")) j.at("value").get_to(obj.value);
                if (j.contains("value_two")) j.at("value_two").get_to(obj.value_two);
                if (j.contains("index")) j.at("index").get_to(obj.index);
        }

        inline void from_json(const nlohmann::json& j, Seccomp::SyscallRule& obj) {
                if (j.contains("args")) j.at("args").get_to(obj.args);
                if (j.contains("names")) j.at("names").get_to(obj.names);
                if (j.contains("action")) j.at("action").get_to(obj.action);
                if (j.contains("errno_ret")) j.at("errno_ret").get_to(obj.errno_ret);
        }

        inline void from_json(const nlohmann::json& j, Seccomp& obj) {
                if (j.contains("syscalls")) j.at("syscalls").get_to(obj.syscalls);
                if (j.contains("archs")) j.at("archs").get_to(obj.archs);
                if (j.contains("flags")) j.at("flags").get_to(obj.flags);
                if (j.contains("default_action")) j.at("default_action").get_to(obj.default_action);
                if (j.contains("default_errno")) j.at("default_errno").get_to(obj.default_errno);
        }

        inline void from_json(const nlohmann::json& j, TimeOffset& obj) {
                if (j.contains("type")) j.at("type").get_to(obj.type);
                if (j.contains("secs")) j.at("secs").get_to(obj.secs);
                if (j.contains("nanosecs")) j.at("nanosecs").get_to(obj.nanosecs);
        }

        inline void from_json(const nlohmann::json& j, Device& obj) {
                if (j.contains("host_path")) obj.host_path = j.at("host_path").get<std::string>();
                if (j.contains("container_path")) obj.container_path = j.at("container_path").get<std::string>();
        }

        inline void from_json(const nlohmann::json& j, Network& obj) {
                if (j.contains("tcp_ports")) j.at("tcp_ports").get_to(obj.tcp_ports);
                if (j.contains("udp_ports")) j.at("udp_ports").get_to(obj.udp_ports);
                if (j.contains("auto_tcp")) j.at("auto_tcp").get_to(obj.auto_tcp);
                if (j.contains("auto_udp")) j.at("auto_udp").get_to(obj.auto_udp);
        }

        inline void from_json(const nlohmann::json& j, Namespace& obj) {
                if (j.contains("path")) obj.path = j.at("path").get<std::string>();
                if (j.contains("type")) j.at("type").get_to(obj.type);
        }

        inline void from_json(const nlohmann::json& j, Mount& obj) {
                if (j.contains("options")) j.at("options").get_to(obj.options);
                if (j.contains("flags")) j.at("flags").get_to(obj.flags);
                if (j.contains("attrs")) j.at("attrs").get_to(obj.attrs);
                if (j.contains("destination")) j.at("destination").get_to(obj.destination);
                if (j.contains("type")) j.at("type").get_to(obj.type);
                if (j.contains("source")) j.at("source").get_to(obj.source);
        }

        inline void from_json(const nlohmann::json& j, MaskedPaths& obj) {
                if (j.contains("paths")) {
                        for (const auto& path_str : j.at("paths")) {
                                obj.paths.push_back(path_str.get<std::string>());
                        }
                }
        }

        inline void from_json(const nlohmann::json& j, ReadOnlyPaths& obj) {
                if (j.contains("paths")) {
                        for (const auto& path_str : j.at("paths")) {
                                obj.paths.push_back(path_str.get<std::string>());
                        }
                }
        }
}

inline void from_json(const nlohmann::json& j, ContainerConfig& obj) {
        if (j.contains("container_id")) j.at("container_id").get_to(obj.container_id);
        if (j.contains("hostname")) j.at("hostname").get_to(obj.hostname);
        if (j.contains("domain_name")) j.at("domain_name").get_to(obj.domain_name);
        if (j.contains("pty_slave_name")) j.at("pty_slave_name").get_to(obj.pty_slave_name);
        if (j.contains("final_filesystem")) j.at("final_filesystem").get_to(obj.final_filesystem);

        if (j.contains("pty_slave_fd")) j.at("pty_slave_fd").get_to(obj.pty_slave_fd);
        if (j.contains("control_sock")) j.at("control_sock").get_to(obj.control_sock);
        if (j.contains("pid")) j.at("pid").get_to(obj.pid);
        if (j.contains("net_pid")) j.at("net_pid").get_to(obj.net_pid);
        if (j.contains("vfs")) j.at("vfs").get_to(obj.vfs);

        if (j.contains("rootfs")) j.at("rootfs").get_to(obj.rootfs);
        if (j.contains("terminal")) j.at("terminal").get_to(obj.terminal);
        if (j.contains("detach")) j.at("detach").get_to(obj.detach);
        if (j.contains("console_size")) j.at("console_size").get_to(obj.console_size);
        if (j.contains("user")) j.at("user").get_to(obj.user);
        if (j.contains("uid_mapping")) j.at("uid_mapping").get_to(obj.uid_mapping);
        if (j.contains("gid_mapping")) j.at("gid_mapping").get_to(obj.gid_mapping);
        if (j.contains("env")) j.at("env").get_to(obj.env);
        if (j.contains("cwd")) j.at("cwd").get_to(obj.cwd);
        if (j.contains("args")) j.at("args").get_to(obj.args);
        if (j.contains("oom_score")) j.at("oom_score").get_to(obj.oom_score);
        if (j.contains("schedular_opts")) j.at("schedular_opts").get_to(obj.schedular_opts);
        if (j.contains("no_new_privileges")) j.at("no_new_privileges").get_to(obj.no_new_privileges);
        if (j.contains("capabilities")) j.at("capabilities").get_to(obj.capabilities);
        if (j.contains("rlimits")) j.at("rlimits").get_to(obj.rlimits);
        if (j.contains("rootfs_propagation")) j.at("rootfs_propagation").get_to(obj.rootfs_propagation);
        if (j.contains("seccomp")) j.at("seccomp").get_to(obj.seccomp);
        if (j.contains("devices")) j.at("devices").get_to(obj.devices);
        if (j.contains("networks")) j.at("networks").get_to(obj.networks);
        if (j.contains("timeoffsets")) j.at("timeoffsets").get_to(obj.timeoffsets);
        if (j.contains("namespaces")) j.at("namespaces").get_to(obj.namespaces);
        if (j.contains("mounts")) j.at("mounts").get_to(obj.mounts);
        if (j.contains("masked_paths")) j.at("masked_paths").get_to(obj.masked_paths);
        if (j.contains("read_only_paths")) j.at("read_only_paths").get_to(obj.read_only_paths);

        if (j.contains("cgroups_path")) obj.cgroups_path = j.at("cgroups_path").get<std::string>();
}

class ConfigParser {
        public:
                static ContainerConfig parse_file(const fs::path& json_file_path) {
                        if (!fs::exists(json_file_path)) {
                                throw std::runtime_error("Config file not found: " + json_file_path.string());
                        }

                        std::ifstream file(json_file_path);
                        nlohmann::json j;
                        file >> j;



                        return j.get<ContainerConfig>();
                }

                static ContainerConfig parse_string(const std::string& json_string) {
                        nlohmann::json j = nlohmann::json::parse(json_string);
                        return j.get<ContainerConfig>();
                }
};
