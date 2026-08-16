#include "serialization.hpp"
#include "container_metadata_generated.h"
#include "types.hpp"
#include <flatbuffers/buffer.h>

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const ContainerDbObject& obj) -> flatbuffers::Offset<FB::ContainerMetadata> {
        auto id_off            { builder.CreateString(obj.config.container_id) };
        auto hostname_off      { builder.CreateString(obj.config.hostname) };
        auto domain_name_off   { builder.CreateString(obj.config.domain_name) };
        auto pty_slave_name_off{ builder.CreateString(obj.config.pty_slave_name) };
        auto cgroups_path_off  { builder.CreateString(obj.config.cgroups_path.string()) };
        auto rootfs_path_off { builder.CreateString(obj.config.rootfs.path.string()) };
        auto rootfs_off      { FB::OCIRuntime::CreateRoot(builder, rootfs_path_off, obj.config.rootfs.read_only) };
        auto rootfs_prop_type_off { builder.CreateString(obj.config.rootfs_propagation.type) };
        auto rootfs_prop_off      { FB::OCIRuntime::CreateRootfsPropagation(builder, rootfs_prop_type_off) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::Mount>> mounts_vec;
        for (const auto& mnt : obj.config.mounts) {
                auto opts_off  { builder.CreateVectorOfStrings(mnt.options) };
                auto flags_off { builder.CreateVectorOfStrings(mnt.flags) };
                auto attrs_off { builder.CreateVectorOfStrings(mnt.attrs) };
                auto dest_off  { builder.CreateString(mnt.destination) };
                auto type_off  { builder.CreateString(mnt.type) };
                auto src_off   { builder.CreateString(mnt.source) };
                mounts_vec.push_back(FB::OCIRuntime::CreateMount(
                                        builder, opts_off, flags_off, attrs_off, dest_off, type_off, src_off));
        }
        auto mounts_off { builder.CreateVector(mounts_vec) };
        std::vector<std::string> masked_strs;
        masked_strs.reserve(obj.config.masked_paths.paths.size());
        for (const auto& p : obj.config.masked_paths.paths) masked_strs.push_back(p.string());
        auto masked_off { FB::OCIRuntime::CreateMaskedPaths(
                        builder, builder.CreateVectorOfStrings(masked_strs)) };
        std::vector<std::string> ro_strs;
        ro_strs.reserve(obj.config.read_only_paths.paths.size());
        for (const auto& p : obj.config.read_only_paths.paths) ro_strs.push_back(p.string());
        auto ro_paths_off { FB::OCIRuntime::CreateReadOnlyPaths(
                        builder, builder.CreateVectorOfStrings(ro_strs)) };
        FB::OCIRuntime::Terminal     terminal_val { obj.config.terminal.value };
        FB::OCIRuntime::Detach       detach_val   { obj.config.detach.value };
        FB::OCIRuntime::ConsoleSize  console_val  { obj.config.console_size.height, obj.config.console_size.width };
        auto add_gids_off { builder.CreateVector(obj.config.user.additional_gids) };
        auto user_off     { FB::OCIRuntime::CreateUser(
                        builder, obj.config.user.uid, obj.config.user.gid, obj.config.user.umask, add_gids_off) };
        auto uid_map_off { FB::OCIRuntime::CreateUidMapping(
                        builder, obj.config.uid_mapping.container_id, obj.config.uid_mapping.host_id, obj.config.uid_mapping.size) };
        auto gid_map_off { FB::OCIRuntime::CreateGidMapping(
                        builder, obj.config.gid_mapping.container_id, obj.config.gid_mapping.host_id, obj.config.gid_mapping.size) };
        auto env_off  { FB::OCIRuntime::CreateEnv(
                        builder, builder.CreateVectorOfStrings(obj.config.env.value)) };
        auto cwd_off  { FB::OCIRuntime::CreateCwd(
                        builder, builder.CreateString(obj.config.cwd.value)) };
        auto args_off { FB::OCIRuntime::CreateArgs(
                        builder, builder.CreateVectorOfStrings(obj.config.args.value)) };
        FB::OCIRuntime::OomScoreAdj      oom_val { obj.config.oom_score.value };
        FB::OCIRuntime::NoNewPrivileges  nnp_val { obj.config.no_new_privileges.value };
        auto sched_flags_off  { builder.CreateVectorOfStrings(obj.config.schedular_opts.flags) };
        auto sched_policy_off { builder.CreateString(obj.config.schedular_opts.policy) };
        auto sched_off        { FB::OCIRuntime::CreateSchedularOpts(
                        builder, sched_flags_off, sched_policy_off,
                        obj.config.schedular_opts.runtime, obj.config.schedular_opts.deadline,
                        obj.config.schedular_opts.period, obj.config.schedular_opts.nice, obj.config.schedular_opts.priority) };
        auto cap_bound_off { builder.CreateVectorOfStrings(obj.config.capabilities.bounding) };
        auto cap_eff_off   { builder.CreateVectorOfStrings(obj.config.capabilities.effective) };
        auto cap_inh_off   { builder.CreateVectorOfStrings(obj.config.capabilities.inheritable) };
        auto cap_perm_off  { builder.CreateVectorOfStrings(obj.config.capabilities.permitted) };
        auto cap_amb_off   { builder.CreateVectorOfStrings(obj.config.capabilities.ambient) };
        auto caps_off      { FB::OCIRuntime::CreateCapabilities(
                        builder, cap_bound_off, cap_eff_off, cap_inh_off, cap_perm_off, cap_amb_off) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::RLimit>> rlimits_vec;
        rlimits_vec.reserve(obj.config.rlimits.size());
        for (const auto& rl : obj.config.rlimits) {
                rlimits_vec.push_back(FB::OCIRuntime::CreateRLimit(
                                        builder, builder.CreateString(rl.name), rl.hard_limit, rl.soft_limit));
        }
        auto rlimits_off { builder.CreateVector(rlimits_vec) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::SyscallRule>> syscall_rules_vec;
        for (const auto& rule : obj.config.seccomp.syscalls) {
                std::vector<flatbuffers::Offset<FB::OCIRuntime::SeccompArg>> args_vec;
                args_vec.reserve(rule.args.size());
                for (const auto& arg : rule.args) {
                        args_vec.push_back(FB::OCIRuntime::CreateSeccompArg(
                                                builder, builder.CreateString(arg.op),
                                                arg.value, arg.value_two, arg.index));
                }
                syscall_rules_vec.push_back(FB::OCIRuntime::CreateSyscallRule(
                                        builder,
                                        builder.CreateVector(args_vec),
                                        builder.CreateVectorOfStrings(rule.names),
                                        builder.CreateString(rule.action),
                                        rule.errno_ret));
        }
        auto seccomp_off { FB::OCIRuntime::CreateSeccomp(
                        builder,
                        builder.CreateVector(syscall_rules_vec),
                        builder.CreateVectorOfStrings(obj.config.seccomp.archs),
                        builder.CreateVectorOfStrings(obj.config.seccomp.flags),
                        builder.CreateString(obj.config.seccomp.default_action),
                        obj.config.seccomp.default_errno) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::Device>> devices_vec;
        devices_vec.reserve(obj.config.devices.size());
        for (const auto& dev : obj.config.devices) {
                devices_vec.push_back(FB::OCIRuntime::CreateDevice(
                                        builder,
                                        builder.CreateString(dev.host_path.string()),
                                        builder.CreateString(dev.container_path.string())));
        }
        auto devices_off { builder.CreateVector(devices_vec) };
        auto tcp_off     { builder.CreateVectorOfStrings(obj.config.networks.tcp_ports) };
        auto udp_off     { builder.CreateVectorOfStrings(obj.config.networks.udp_ports) };
        auto networks_off { FB::OCIRuntime::CreateNetwork(
                        builder, tcp_off, udp_off, obj.config.networks.auto_tcp, obj.config.networks.auto_udp) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::TimeOffset>> timeoff_vec;
        timeoff_vec.reserve(obj.config.timeoffsets.size());
        for (const auto& to : obj.config.timeoffsets) {
                timeoff_vec.emplace_back(FB::OCIRuntime::CreateTimeOffset(
                                        builder, builder.CreateString(to.type), to.secs, to.nanosecs));
        }
        auto timeoffsets_off { builder.CreateVector(timeoff_vec) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::Namespace>> ns_vec;
        ns_vec.reserve(obj.config.namespaces.size());
        for (const auto& ns : obj.config.namespaces) {
                ns_vec.emplace_back(FB::OCIRuntime::CreateNamespace(
                                        builder,
                                        builder.CreateString(ns.path.string()),
                                        builder.CreateString(ns.type)));
        }
        auto namespaces_off { builder.CreateVector(ns_vec) };
        FB::ContainerConfigBuilder fb_builder { builder };
        fb_builder.add_container_id(id_off);
        fb_builder.add_hostname(hostname_off);
        fb_builder.add_domain_name(domain_name_off);
        fb_builder.add_pty_slave_name(pty_slave_name_off);
        fb_builder.add_pty_slave_fd(obj.config.pty_slave_fd);
        fb_builder.add_control_sock(obj.config.control_sock);
        fb_builder.add_pid(obj.config.pid);
        fb_builder.add_net_pid(obj.config.net_pid);
        fb_builder.add_vfs(obj.config.vfs);
        fb_builder.add_rootfs(rootfs_off);
        fb_builder.add_rootfs_propagation(rootfs_prop_off);
        fb_builder.add_cgroups_path(cgroups_path_off);
        fb_builder.add_mounts(mounts_off);
        fb_builder.add_masked_paths(masked_off);
        fb_builder.add_read_only_paths(ro_paths_off);
        fb_builder.add_terminal(&terminal_val);
        fb_builder.add_detach(&detach_val);
        fb_builder.add_console_size(&console_val);
        fb_builder.add_user(user_off);
        fb_builder.add_uid_mapping(uid_map_off);
        fb_builder.add_gid_mapping(gid_map_off);
        fb_builder.add_env(env_off);
        fb_builder.add_cwd(cwd_off);
        fb_builder.add_args(args_off);
        fb_builder.add_oom_score(&oom_val);
        fb_builder.add_schedular_opts(sched_off);
        fb_builder.add_no_new_privileges(&nnp_val);
        fb_builder.add_capabilities(caps_off);
        fb_builder.add_rlimits(rlimits_off);
        fb_builder.add_seccomp(seccomp_off);
        fb_builder.add_devices(devices_off);
        fb_builder.add_networks(networks_off);
        fb_builder.add_namespaces(namespaces_off);
        fb_builder.add_timeoffsets(timeoffsets_off);
        auto config_off = fb_builder.Finish();

        std::vector<flatbuffers::Offset<FB::IOMaxUpdate>> io_max_vec{};
        io_max_vec.reserve(obj.io_max_updates.size());
        for (const auto& im : obj.io_max_updates) {
                io_max_vec.push_back(FB::CreateIOMaxUpdate(builder, im.major, im.minor,
                                        im.limits.rbps, im.limits.wbps, im.limits.riops, im.limits.wiops));
        }
        auto io_max_off{builder.CreateVector(io_max_vec)};

        std::vector<flatbuffers::Offset<FB::IOWeightUpdate>> io_weight_vec{};
        io_weight_vec.reserve(obj.io_weight_updates.size());
        for (const auto& iw : obj.io_weight_updates) {
                io_weight_vec.push_back(FB::CreateIOWeightUpdate(builder, iw.major, iw.minor, iw.weight));
        }
        auto io_weight_off{builder.CreateVector(io_weight_vec)};
        auto name_off{builder.CreateString(obj.name)};
        auto image_off{builder.CreateString(obj.image)};
        auto status_off{builder.CreateString(obj.status)};
        auto created_at_off {builder.CreateString(obj.created_at)};
        auto final_filesystem_off{builder.CreateString(obj.config.final_filesystem)};
        auto cpusets_off{builder.CreateString(obj.cpuset_cpus)};
        auto cpusmems_off{builder.CreateString(obj.cpuset_mems)};

        FB::ContainerMetadataBuilder meta_builder { builder };
        meta_builder.add_config(config_off);
        meta_builder.add_name(name_off);
        meta_builder.add_image(image_off);
        meta_builder.add_status(status_off);
        meta_builder.add_created_at(created_at_off);
        meta_builder.add_final_filesystem(final_filesystem_off);
        meta_builder.add_boot_time(obj.boot_time);
        meta_builder.add_cpu_quota(obj.cpu_quota);
        meta_builder.add_cpu_period(obj.cpu_period);
        meta_builder.add_cpu_weight(obj.cpu_weight);
        meta_builder.add_memory_max(obj.memory_max);
        meta_builder.add_memory_swap(obj.memory_swap);
        meta_builder.add_cpuset_cpus(cpusets_off);
        meta_builder.add_cpuset_mems(cpusmems_off);
        meta_builder.add_io_max_updates(io_max_off);
        meta_builder.add_io_weight_updates(io_weight_off);
        return meta_builder.Finish();
}
auto Serialization::deserialize(const FB::ContainerMetadata* fb) -> ContainerDbObject {
        ContainerDbObject obj{};
        if (!fb) return obj;
        if (fb->name()) obj.name = fb->name()->str();
        if (fb->image()) obj.image = fb->image()->str();
        if (fb->status()) obj.status = fb->status()->str();
        if (fb->boot_time()) obj.boot_time = fb->boot_time();
        if (fb->created_at()) obj.created_at = fb->created_at()->str();
        if (fb->cpu_quota()) obj.cpu_quota = fb->cpu_quota();
        if (fb->cpu_period()) obj.cpu_period = fb->cpu_period();
        if (fb->cpu_weight()) obj.cpu_weight = fb->cpu_weight();
        if (fb->memory_max()) obj.memory_max = fb->memory_max();
        if (fb->memory_swap()) obj.memory_swap = fb->memory_swap();
        if (fb->cpuset_cpus()) obj.cpuset_cpus = fb->cpuset_cpus()->str();
        if (fb->cpuset_mems()) obj.cpuset_mems = fb->cpuset_mems()->str();

        if (fb->io_max_updates()) {
                for (const auto* io_max_update : *fb->io_max_updates()) {
                        IOMaxUpdate io_update{};
                        io_update.minor = io_max_update->minor();
                        io_update.major = io_max_update->major();
                        io_update.limits.rbps = io_max_update->rbps();
                        io_update.limits.riops = io_max_update->riops();
                        io_update.limits.wbps = io_max_update->wbps();
                        io_update.limits.wiops = io_max_update->wiops();
                        obj.io_max_updates.emplace_back(io_update);
                }
        }

        if (fb->io_weight_updates()) {
                for (const auto* io_weight_update : *fb->io_weight_updates()) {
                        IOWeightUpdate io_update{};
                        io_update.minor = io_weight_update->minor();
                        io_update.major = io_weight_update->major();
                        io_update.weight = io_weight_update->weight();
                        obj.io_weight_updates.emplace_back(io_update);
                }
        }

        auto fb_conf = fb->config();
        if (!fb_conf) return obj;
        obj.config.pty_slave_fd  = fb_conf->pty_slave_fd();
        obj.config.control_sock  = fb_conf->control_sock();
        obj.config.pid           = fb_conf->pid();
        obj.config.net_pid       = fb_conf->net_pid();
        obj.config.vfs           = fb_conf->vfs();
        if (fb_conf->container_id())   obj.config.container_id   = fb_conf->container_id()->str();
        if (fb_conf->hostname())       obj.config.hostname        = fb_conf->hostname()->str();
        if (fb_conf->domain_name())    obj.config.domain_name     = fb_conf->domain_name()->str();
        if (fb_conf->pty_slave_name()) obj.config.pty_slave_name  = fb_conf->pty_slave_name()->str();
        if (fb_conf->cgroups_path())   obj.config.cgroups_path    = fb_conf->cgroups_path()->str();
        if (const auto* r = fb_conf->rootfs()) {
                if (r->path()) obj.config.rootfs.path = r->path()->str();
                obj.config.rootfs.read_only = r->read_only();
        }
        if (const auto* rp = fb_conf->rootfs_propagation()) {
                if (rp->type()) obj.config.rootfs_propagation.type = rp->type()->str();
        }
        if (const auto* t = fb_conf->terminal()) obj.config.terminal.value         = t->value();
        if (const auto* d = fb_conf->detach())   obj.config.detach.value           = d->value();
        if (const auto* c = fb_conf->console_size()) {
                obj.config.console_size.height = c->height();
                obj.config.console_size.width  = c->width();
        }
        if (const auto* u = fb_conf->user()) {
                obj.config.user.uid   = u->uid();
                obj.config.user.gid   = u->gid();
                obj.config.user.umask = u->umask();
                if (u->additional_gids()) {
                        obj.config.user.additional_gids.assign(
                                        u->additional_gids()->begin(), u->additional_gids()->end());
                }
        }
        if (const auto* um = fb_conf->uid_mapping()) {
                obj.config.uid_mapping.container_id = um->container_id();
                obj.config.uid_mapping.host_id      = um->host_id();
                obj.config.uid_mapping.size         = um->size();
        }
        if (const auto* gm = fb_conf->gid_mapping()) {
                obj.config.gid_mapping.container_id = gm->container_id();
                obj.config.gid_mapping.host_id      = gm->host_id();
                obj.config.gid_mapping.size         = gm->size();
        }
        if (const auto* e = fb_conf->env()) {
                if (e->value()) {
                        for (const auto* s : *e->value()) obj.config.env.value.push_back(s->str());
                }
        }
        if (const auto* c = fb_conf->cwd()) {
                if (c->value()) obj.config.cwd.value = c->value()->str();
        }
        if (const auto* a = fb_conf->args()) {
                if (a->value()) {
                        for (const auto* s : *a->value()) obj.config.args.value.push_back(s->str());
                }
        }
        if (const auto* o = fb_conf->oom_score())         obj.config.oom_score.value         = o->value();
        if (const auto* n = fb_conf->no_new_privileges()) obj.config.no_new_privileges.value = n->value();
        if (const auto* s = fb_conf->schedular_opts()) {
                if (s->flags())  for (const auto* f : *s->flags())  obj.config.schedular_opts.flags.push_back(f->str());
                if (s->policy()) obj.config.schedular_opts.policy   = s->policy()->str();
                obj.config.schedular_opts.runtime  = s->runtime();
                obj.config.schedular_opts.deadline = s->deadline();
                obj.config.schedular_opts.period   = s->period();
                obj.config.schedular_opts.nice     = s->nice();
                obj.config.schedular_opts.priority = s->priority();
        }
        if (const auto* c = fb_conf->capabilities()) {
                auto copy = [](const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* v,
                                std::vector<std::string>& out) {
                        if (v) for (const auto* s : *v) out.push_back(s->str());
                };
                copy(c->bounding(),    obj.config.capabilities.bounding);
                copy(c->effective(),   obj.config.capabilities.effective);
                copy(c->inheritable(), obj.config.capabilities.inheritable);
                copy(c->permitted(),   obj.config.capabilities.permitted);
                copy(c->ambient(),     obj.config.capabilities.ambient);
        }
        if (fb_conf->rlimits()) {
                for (const auto* rl : *fb_conf->rlimits()) {
                        OCIRuntime::RLimit r{};
                        if (rl->name()) r.name = rl->name()->str();
                        r.hard_limit = rl->hard_limit();
                        r.soft_limit = rl->soft_limit();
                        obj.config.rlimits.push_back(std::move(r));
                }
        }
        if (const auto* sc = fb_conf->seccomp()) {
                if (sc->archs())          for (const auto* s : *sc->archs())  obj.config.seccomp.archs.push_back(s->str());
                if (sc->flags())          for (const auto* s : *sc->flags())  obj.config.seccomp.flags.push_back(s->str());
                if (sc->default_action()) obj.config.seccomp.default_action = sc->default_action()->str();
                obj.config.seccomp.default_errno = sc->default_errno();
                if (sc->syscalls()) {
                        for (const auto* rule : *sc->syscalls()) {
                                OCIRuntime::Seccomp::SyscallRule sr{};
                                if (rule->names())  for (const auto* n : *rule->names()) sr.names.push_back(n->str());
                                if (rule->action()) sr.action = rule->action()->str();
                                sr.errno_ret = rule->errno_ret();
                                if (rule->args()) {
                                        for (const auto* arg : *rule->args()) {
                                                OCIRuntime::Seccomp::Arg a{};
                                                if (arg->op()) a.op = arg->op()->str();
                                                a.value     = arg->value();
                                                a.value_two = arg->value_two();
                                                a.index     = arg->index();
                                                sr.args.push_back(std::move(a));
                                        }
                                }
                                obj.config.seccomp.syscalls.push_back(std::move(sr));
                        }
                }
        }
        if (fb_conf->devices()) {
                for (const auto* dev : *fb_conf->devices()) {
                        OCIRuntime::Device d{};
                        if (dev->host_path())      d.host_path      = dev->host_path()->str();
                        if (dev->container_path()) d.container_path = dev->container_path()->str();
                        obj.config.devices.push_back(std::move(d));
                }
        }
        if (const auto* net = fb_conf->networks()) {
                if (net->tcp_ports()) for (const auto* p : *net->tcp_ports()) obj.config.networks.tcp_ports.push_back(p->str());
                if (net->udp_ports()) for (const auto* p : *net->udp_ports()) obj.config.networks.udp_ports.push_back(p->str());
                obj.config.networks.auto_tcp = net->auto_tcp();
                obj.config.networks.auto_udp = net->auto_udp();
        }
        if (fb_conf->timeoffsets()) {
                for (const auto* to : *fb_conf->timeoffsets()) {
                        OCIRuntime::TimeOffset t{};
                        if (to->type()) t.type = to->type()->str();
                        t.secs    = to->secs();
                        t.nanosecs = to->nanosecs();
                        obj.config.timeoffsets.push_back(std::move(t));
                }
        }
        if (fb_conf->namespaces()) {
                for (const auto* ns : *fb_conf->namespaces()) {
                        OCIRuntime::Namespace n{};
                        if (ns->path()) n.path = ns->path()->str();
                        if (ns->type()) n.type = ns->type()->str();
                        obj.config.namespaces.push_back(std::move(n));
                }
        }
        if (fb_conf->mounts()) {
                for (const auto* mnt : *fb_conf->mounts()) {
                        OCIRuntime::Mount m{};
                        if (mnt->options())     for (const auto* s : *mnt->options()) m.options.push_back(s->str());
                        if (mnt->flags())       for (const auto* s : *mnt->flags())   m.flags.push_back(s->str());
                        if (mnt->attrs())       for (const auto* s : *mnt->attrs())   m.attrs.push_back(s->str());
                        if (mnt->destination()) m.destination = mnt->destination()->str();
                        if (mnt->type())        m.type        = mnt->type()->str();
                        if (mnt->source())      m.source      = mnt->source()->str();
                        obj.config.mounts.push_back(std::move(m));
                }
        }
        if (fb_conf->masked_paths() && fb_conf->masked_paths()->paths()) {
                for (const auto* p : *fb_conf->masked_paths()->paths())
                        obj.config.masked_paths.paths.push_back(p->str());
        }
        if (fb_conf->read_only_paths() && fb_conf->read_only_paths()->paths()) {
                for (const auto* p : *fb_conf->read_only_paths()->paths())
                        obj.config.read_only_paths.paths.push_back(p->str());
        }
        return obj;
}
auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const ImageMetadata& obj) -> flatbuffers::Offset<FB::ImageMetadata> {
        auto id_off           { builder.CreateString(obj.id) };
        auto name_off         { builder.CreateString(obj.name) };
        auto tag_off          { builder.CreateString(obj.tag) };
        auto source_off       { builder.CreateString(obj.source) };
        FB::ImageMetadataBuilder fb_builder { builder };
        fb_builder.add_id(id_off);
        fb_builder.add_name(name_off);
        fb_builder.add_tag(tag_off);
        fb_builder.add_size_bytes(obj.size_bytes);
        fb_builder.add_source(source_off);
        return fb_builder.Finish();
}
auto Serialization::deserialize(const FB::ImageMetadata* fb) -> ImageMetadata {
        ImageMetadata obj{};
        if (!fb) return obj;
        if (fb->id()) obj.id = fb->id()->str();
        if (fb->name()) obj.name = fb->name()->str();
        if (fb->tag()) obj.tag = fb->tag()->str();
        if (fb->source()) obj.source = fb->source()->str();
        obj.size_bytes = fb->size_bytes();
        return obj;
}
