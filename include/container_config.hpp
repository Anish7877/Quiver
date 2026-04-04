#pragma once
#include "oci_runtime.hpp"
#include <vector>

struct ContainerConfig {
        std::string container_id{};
        std::string hostname{};
        std::string domain_name{};
        pid_t pid{};
        bool vfs{};
        OCIRuntime::Root rootfs{};
        OCIRuntime::Terminal terminal{};
        OCIRuntime::ConsoleSize console_size{};
        OCIRuntime::User user{};
        OCIRuntime::UidMapping uid_mapping{};
        OCIRuntime::GidMapping gid_mapping{};
        OCIRuntime::Env env{};
        OCIRuntime::Cwd cwd{};
        OCIRuntime::Args args{};
        OCIRuntime::OomScoreAdj oom_score{};
        OCIRuntime::SchedularOpts schedular_opts{};
        OCIRuntime::IOPriority io_priority{};
        OCIRuntime::NoNewPrivileges no_new_privileges{};
        OCIRuntime::Capabilities capabilities{};
        std::vector<OCIRuntime::RLimit> rlimits{};
        OCIRuntime::CPUAffnity cpu_affinity{};
        OCIRuntime::Hooks hooks{};
        OCIRuntime::RootfsPropagation rootfs_propagation{};
        OCIRuntime::Seccomp seccomp{};
        fs::path cgroups_path{};
        std::vector<OCIRuntime::TimeOffset> timeoffsets{};
        std::vector<OCIRuntime::Namespace> namespaces{};
        std::vector<OCIRuntime::Mount> mounts{};
        std::vector<OCIRuntime::MaskedPath> masked_paths{};
        std::vector<OCIRuntime::ReadOnlyPath> read_only_paths{};
        std::unordered_map<std::string, std::string> annotations{};
};
