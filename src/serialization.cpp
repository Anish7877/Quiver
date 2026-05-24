#include "serialization.hpp"

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const ContainerConfig& obj) -> flatbuffers::Offset<Types::Container> {
        auto id_off{builder.CreateString(obj.container_id)};
        auto hostname_off{builder.CreateString(obj.hostname)};
        auto domain_name_off{builder.CreateString(obj.domain_name)};
        auto cwd_off{builder.CreateString(obj.cwd.value)};
        auto cgroups_path_off{builder.CreateString(obj.cgroups_path.string())};
        auto rootfs_prop_off{builder.CreateString(obj.rootfs_propagation.type)};

        auto env_off{builder.CreateVectorOfStrings(obj.env.value)};
        auto args_off{builder.CreateVectorOfStrings(obj.args.value)};

        auto rootfs_path_off{builder.CreateString(obj.rootfs.path.string())};
        auto rootfs_off{Types::CreateRoot(builder, rootfs_path_off, obj.rootfs.read_only)};

        auto console_size_off{Types::CreateConsoleSize(builder, obj.console_size.height, obj.console_size.width)};

        auto add_gids_off{builder.CreateVector(obj.user.additional_gids)};
        auto user_off{Types::CreateUser(builder, obj.user.uid, obj.user.gid, obj.user.umask, add_gids_off)};

        auto uid_map_off{Types::CreateIdMapping(builder, obj.uid_mapping.container_id, obj.uid_mapping.host_id, obj.uid_mapping.size)};
        auto gid_map_off{Types::CreateIdMapping(builder, obj.gid_mapping.container_id, obj.gid_mapping.host_id, obj.gid_mapping.size)};

        auto sched_flags_off{builder.CreateVectorOfStrings(obj.schedular_opts.flags)};
        auto sched_policy_off{builder.CreateString(obj.schedular_opts.policy)};
        auto sched_opts_off{Types::CreateSchedularOpts(builder, sched_flags_off, sched_policy_off,
                obj.schedular_opts.runtime, obj.schedular_opts.deadline, obj.schedular_opts.period,
                obj.schedular_opts.nice, obj.schedular_opts.priority)};

        auto cap_bound_off{builder.CreateVectorOfStrings(obj.capabilities.bounding)};
        auto cap_eff_off{builder.CreateVectorOfStrings(obj.capabilities.effective)};
        auto cap_inh_off{builder.CreateVectorOfStrings(obj.capabilities.inheritable)};
        auto cap_perm_off{builder.CreateVectorOfStrings(obj.capabilities.permitted)};
        auto cap_amb_off{builder.CreateVectorOfStrings(obj.capabilities.ambient)};
        auto caps_off{Types::CreateCapabilities(builder, cap_bound_off, cap_eff_off, cap_inh_off, cap_perm_off, cap_amb_off)};

        std::vector<flatbuffers::Offset<Types::RLimit>> rlimits_vec;
        for (const auto& rl : obj.rlimits) {
                rlimits_vec.push_back(Types::CreateRLimit(builder, builder.CreateString(rl.name), rl.hard_limit, rl.soft_limit));
        }
        auto rlimits_off{builder.CreateVector(rlimits_vec)};

        std::vector<flatbuffers::Offset<Types::SeccompSyscallRule>> syscall_rules_vec;
        for (const auto& rule : obj.seccomp.syscalls) {
                std::vector<flatbuffers::Offset<Types::SeccompArg>> args_vec;
                for (const auto& arg : rule.args) {
                        args_vec.push_back(Types::CreateSeccompArg(builder, builder.CreateString(arg.op), arg.value, arg.value_two, arg.index));
                }
                auto seccomp_args_off{builder.CreateVector(args_vec)};
                auto names_off{builder.CreateVectorOfStrings(rule.names)};
                auto action_off{builder.CreateString(rule.action)};
                syscall_rules_vec.push_back(Types::CreateSeccompSyscallRule(builder, seccomp_args_off, names_off, action_off, rule.errno_ret));
        }
        auto syscalls_off{builder.CreateVector(syscall_rules_vec)};
        auto seccomp_archs_off{builder.CreateVectorOfStrings(obj.seccomp.archs)};
        auto seccomp_flags_off{builder.CreateVectorOfStrings(obj.seccomp.flags)};
        auto seccomp_def_action_off{builder.CreateString(obj.seccomp.default_action)};
        auto seccomp_off{Types::CreateSeccomp(builder, syscalls_off, seccomp_archs_off, seccomp_flags_off, seccomp_def_action_off, obj.seccomp.default_errno)};

        std::vector<flatbuffers::Offset<Types::Device>> devices_vec;
        for (const auto& dev : obj.devices) {
                devices_vec.push_back(Types::CreateDevice(builder, builder.CreateString(dev.host_path.string()), builder.CreateString(dev.container_path.string())));
        }
        auto devices_off{builder.CreateVector(devices_vec)};

        auto tcp_off{builder.CreateVectorOfStrings(obj.networks.tcp_ports)};
        auto udp_off{builder.CreateVectorOfStrings(obj.networks.udp_ports)};
        auto network_off{Types::CreateNetwork(builder, tcp_off, udp_off, obj.networks.auto_tcp, obj.networks.auto_udp)};

        std::vector<flatbuffers::Offset<Types::TimeOffset>> timeoff_vec;
        for (const auto& to : obj.timeoffsets) {
                timeoff_vec.push_back(Types::CreateTimeOffset(builder, builder.CreateString(to.type), to.secs, to.nanosecs));
        }
        auto timeoffsets_off{builder.CreateVector(timeoff_vec)};

        std::vector<flatbuffers::Offset<Types::Namespace>> ns_vec;
        for (const auto& ns : obj.namespaces) {
                ns_vec.push_back(Types::CreateNamespace(builder, builder.CreateString(ns.path.string()), builder.CreateString(ns.type)));
        }
        auto namespaces_off{builder.CreateVector(ns_vec)};

        std::vector<flatbuffers::Offset<Types::Mount>> mounts_vec;
        for (const auto& mnt : obj.mounts) {
                auto opts_off = builder.CreateVectorOfStrings(mnt.options);
                auto m_flags_off = builder.CreateVectorOfStrings(mnt.flags);
                auto attrs_off = builder.CreateVectorOfStrings(mnt.attrs);
                mounts_vec.push_back(Types::CreateMount(builder, opts_off, m_flags_off, attrs_off,
                        builder.CreateString(mnt.destination), builder.CreateString(mnt.type), builder.CreateString(mnt.source)));
        }
        auto mounts_off{builder.CreateVector(mounts_vec)};

        std::vector<std::string> masked_strs;
        for (const auto& p : obj.masked_paths.paths) masked_strs.push_back(p.string());
        auto masked_paths_off{builder.CreateVectorOfStrings(masked_strs)};

        std::vector<std::string> ro_strs;
        for (const auto& p : obj.read_only_paths.paths) ro_strs.push_back(p.string());
        auto ro_paths_off{builder.CreateVectorOfStrings(ro_strs)};

        Types::ContainerBuilder fb_builder{builder};

        fb_builder.add_id(id_off);
        fb_builder.add_hostname(hostname_off);
        fb_builder.add_domain_name(domain_name_off);
        fb_builder.add_pid(obj.pid);
        fb_builder.add_net_pid(obj.net_pid);
        fb_builder.add_vfs(obj.vfs);

        fb_builder.add_rootfs(rootfs_off);
        fb_builder.add_terminal(obj.terminal.value);
        fb_builder.add_console_size(console_size_off);
        fb_builder.add_user(user_off);
        fb_builder.add_uid_mapping(uid_map_off);
        fb_builder.add_gid_mapping(gid_map_off);
        fb_builder.add_env(env_off);
        fb_builder.add_cwd(cwd_off);
        fb_builder.add_args(args_off);
        fb_builder.add_oom_score(obj.oom_score.value);
        fb_builder.add_schedular_opts(sched_opts_off);
        fb_builder.add_no_new_privileges(obj.no_new_privileges.value);
        fb_builder.add_capabilities(caps_off);
        fb_builder.add_rlimits(rlimits_off);
        fb_builder.add_rootfs_propagation(rootfs_prop_off);
        fb_builder.add_seccomp(seccomp_off);
        fb_builder.add_cgroups_path(cgroups_path_off);

        fb_builder.add_devices(devices_off);
        fb_builder.add_networks(network_off);
        fb_builder.add_timeoffsets(timeoffsets_off);
        fb_builder.add_namespaces(namespaces_off);
        fb_builder.add_mounts(mounts_off);
        fb_builder.add_masked_paths(masked_paths_off);
        fb_builder.add_read_only_paths(ro_paths_off);

        return fb_builder.Finish();
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const VolumeType& obj) -> flatbuffers::Offset<Types::Volume> {
        std::vector<flatbuffers::Offset<Types::PairString>> vols_offset{};
        for (const auto& vol : obj.volumes) {
                vols_offset.emplace_back(Types::CreatePairString(builder,
                                        builder.CreateString(vol.first),
                                        builder.CreateString(vol.second)));
        }
        auto id_offset{builder.CreateString(obj.container_id)};
        auto created_at_offset{builder.CreateString(obj.created_at)};
        auto vols_vec_offset{builder.CreateVector(vols_offset)};
        Types::VolumeBuilder fb_builder{builder};
        fb_builder.add_container_id(id_offset);
        fb_builder.add_created_at(created_at_offset);
        fb_builder.add_paths(vols_vec_offset);
        return fb_builder.Finish();
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const DeviceType& obj) -> flatbuffers::Offset<Types::Device> {
        auto id_offset{builder.CreateString(obj.container_id)};
        auto created_at_offset{builder.CreateString(obj.created_at)};
        auto devs_vec_offset{builder.CreateVectorOfStrings(obj.devices)};
        Types::DeviceBuilder fb_builder{builder};
        fb_builder.add_container_id(id_offset);
        fb_builder.add_created_at(created_at_offset);
        fb_builder.add_paths(devs_vec_offset);
        return fb_builder.Finish();
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const NetworkType& obj) -> flatbuffers::Offset<Types::Network> {
        std::vector<flatbuffers::Offset<Types::PairInt32>> ports_offset{};
        for (const auto& port : obj.ports) {
                ports_offset.emplace_back(Types::CreatePairInt32(builder,
                                        port.first, port.second));
        }
        auto id_offset{builder.CreateString(obj.container_id)};
        auto created_at_offset{builder.CreateString(obj.created_at)};
        auto ports_vec_offset{builder.CreateVector(ports_offset)};
        Types::NetworkBuilder fb_builder{builder};
        fb_builder.add_container_id(id_offset);
        fb_builder.add_created_at(created_at_offset);
        fb_builder.add_ports(ports_vec_offset);
        return fb_builder.Finish();
}

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const ImageType& obj) -> flatbuffers::Offset<Types::Image> {
        auto id_offset{builder.CreateString(obj.id)};
        auto name_offset{builder.CreateString(obj.name)};
        auto tag_offset{builder.CreateString(obj.tag)};
        auto path_offset{builder.CreateString(obj.path)};
        auto created_at_offset{builder.CreateString(obj.created_at)};
        Types::ImageBuilder fb_builder{builder};
        return fb_builder.Finish();
}

auto Serialization::deserialize(const Types::Container* fb) -> ContainerType {
        ContainerType obj{};
        if(!fb) return obj;

        obj.pid = fb->pid();
        obj.net_pid = fb->net_pid();
        obj.vfs = fb->vfs();
        obj.no_remove = fb->no_remove();

        if (fb->id()) obj.id = fb->id()->str();
        if (fb->name()) obj.name = fb->name()->str();
        if (fb->image()) obj.image = fb->image()->str();
        if (fb->status()) obj.status = fb->status()->str();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        if (fb->hostname()) obj.hostname = fb->hostname()->str();
        if (fb->filesystem_path()) obj.filesystem_path = fb->filesystem_path()->str();
        if (fb->vfs_path()) obj.vfs_path = fb->vfs_path()->str();

        if (fb->volumes()) {
                for (const auto* vol : *fb->volumes()) {
                        obj.volumes.emplace_back(std::make_pair(vol->host_path() ? vol->host_path()->str() : "",
                                                                vol->container_path() ? vol->container_path()->str() : ""));
                }
        }

        if (fb->ports()) {
                for (const auto* port : *fb->ports()) {
                        obj.ports.emplace_back(std::make_pair(port->host_port(), port->container_port()));
                }
        }

        if (fb->devices()) {
                for (const auto* dev : *fb->devices()) {
                        obj.devices.emplace_back(dev->str());
                }
        }

        return obj;
}

auto Serialization::deserialize(const Types::Volume* fb) -> VolumeType {
        VolumeType obj{};
        if (!fb) return obj;
        if (fb->container_id()) {obj.container_id =  fb->container_id()->str();}
        if (fb->created_at()) {obj.created_at = fb->created_at()->str();}
        if (fb->paths()) {
                for (const auto* vol : *fb->paths()) {
                        obj.volumes.emplace_back(std::make_pair(vol->host_path() ? vol->host_path()->str() : "",
                                                                vol->container_path() ? vol->container_path()->str(): ""));
                }
        }
        return obj;
}

auto Serialization::deserialize(const Types::Device* fb) -> DeviceType {
        DeviceType obj{};
        if (!fb) return obj;
        if (fb->container_id()) obj.container_id = fb->container_id()->str();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        if (fb->paths()) {
                for (const auto* dev : *fb->paths()) {
                        obj.devices.emplace_back(dev->str());
                }
        }
        return obj;
}

auto Serialization::deserialize(const Types::Network* fb) -> NetworkType {
        NetworkType obj{};
        if (!fb) return obj;
        if (fb->container_id()) obj.container_id = fb->container_id()->str();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        if (fb->ports()) {
                for(const auto* port : *fb->ports()) {
                        obj.ports.emplace_back(std::make_pair(port->host_port(), port->container_port()));
                }
        }
        return obj;
}

auto Serialization::deserialize(const Types::Image* fb) -> ImageType {
        ImageType obj{};
        if (!fb) return obj;
        if (fb->id()) obj.id = fb->id()->str();
        if (fb->name()) obj.name = fb->name()->str();
        if (fb->tag()) obj.tag = fb->tag()->str();
        if (fb->path()) obj.path = fb->path()->str();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        return obj;
}
