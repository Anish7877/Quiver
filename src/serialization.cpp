#include "serialization.hpp"

auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const ContainerConfig& obj) -> flatbuffers::Offset<FB::ContainerConfig> {
        auto id_off            { builder.CreateString(obj.container_id) };
        auto hostname_off      { builder.CreateString(obj.hostname) };
        auto domain_name_off   { builder.CreateString(obj.domain_name) };
        auto pty_slave_name_off{ builder.CreateString(obj.pty_slave_name) };
        auto cgroups_path_off  { builder.CreateString(obj.cgroups_path.string()) };
        auto rootfs_path_off { builder.CreateString(obj.rootfs.path.string()) };
        auto rootfs_off      { FB::OCIRuntime::CreateRoot(builder, rootfs_path_off, obj.rootfs.read_only) };
        auto rootfs_prop_type_off { builder.CreateString(obj.rootfs_propagation.type) };
        auto rootfs_prop_off      { FB::OCIRuntime::CreateRootfsPropagation(builder, rootfs_prop_type_off) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::Mount>> mounts_vec;
        for (const auto& mnt : obj.mounts) {
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
        masked_strs.reserve(obj.masked_paths.paths.size());
        for (const auto& p : obj.masked_paths.paths) masked_strs.push_back(p.string());
        auto masked_off { FB::OCIRuntime::CreateMaskedPaths(
                        builder, builder.CreateVectorOfStrings(masked_strs)) };
        std::vector<std::string> ro_strs;
        ro_strs.reserve(obj.read_only_paths.paths.size());
        for (const auto& p : obj.read_only_paths.paths) ro_strs.push_back(p.string());
        auto ro_paths_off { FB::OCIRuntime::CreateReadOnlyPaths(
                        builder, builder.CreateVectorOfStrings(ro_strs)) };
        FB::OCIRuntime::Terminal     terminal_val { obj.terminal.value };
        FB::OCIRuntime::Detach       detach_val   { obj.detach.value };
        FB::OCIRuntime::ConsoleSize  console_val  { obj.console_size.height, obj.console_size.width };
        auto add_gids_off { builder.CreateVector(obj.user.additional_gids) };
        auto user_off     { FB::OCIRuntime::CreateUser(
                        builder, obj.user.uid, obj.user.gid, obj.user.umask, add_gids_off) };
        auto uid_map_off { FB::OCIRuntime::CreateUidMapping(
                        builder, obj.uid_mapping.container_id, obj.uid_mapping.host_id, obj.uid_mapping.size) };
        auto gid_map_off { FB::OCIRuntime::CreateGidMapping(
                        builder, obj.gid_mapping.container_id, obj.gid_mapping.host_id, obj.gid_mapping.size) };
        auto env_off  { FB::OCIRuntime::CreateEnv(
                        builder, builder.CreateVectorOfStrings(obj.env.value)) };
        auto cwd_off  { FB::OCIRuntime::CreateCwd(
                        builder, builder.CreateString(obj.cwd.value)) };
        auto args_off { FB::OCIRuntime::CreateArgs(
                        builder, builder.CreateVectorOfStrings(obj.args.value)) };
        FB::OCIRuntime::OomScoreAdj      oom_val { obj.oom_score.value };
        FB::OCIRuntime::NoNewPrivileges  nnp_val { obj.no_new_privileges.value };
        auto sched_flags_off  { builder.CreateVectorOfStrings(obj.schedular_opts.flags) };
        auto sched_policy_off { builder.CreateString(obj.schedular_opts.policy) };
        auto sched_off        { FB::OCIRuntime::CreateSchedularOpts(
                        builder, sched_flags_off, sched_policy_off,
                        obj.schedular_opts.runtime, obj.schedular_opts.deadline,
                        obj.schedular_opts.period, obj.schedular_opts.nice, obj.schedular_opts.priority) };
        auto cap_bound_off { builder.CreateVectorOfStrings(obj.capabilities.bounding) };
        auto cap_eff_off   { builder.CreateVectorOfStrings(obj.capabilities.effective) };
        auto cap_inh_off   { builder.CreateVectorOfStrings(obj.capabilities.inheritable) };
        auto cap_perm_off  { builder.CreateVectorOfStrings(obj.capabilities.permitted) };
        auto cap_amb_off   { builder.CreateVectorOfStrings(obj.capabilities.ambient) };
        auto caps_off      { FB::OCIRuntime::CreateCapabilities(
                        builder, cap_bound_off, cap_eff_off, cap_inh_off, cap_perm_off, cap_amb_off) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::RLimit>> rlimits_vec;
        rlimits_vec.reserve(obj.rlimits.size());
        for (const auto& rl : obj.rlimits) {
                rlimits_vec.push_back(FB::OCIRuntime::CreateRLimit(
                                        builder, builder.CreateString(rl.name), rl.hard_limit, rl.soft_limit));
        }
        auto rlimits_off { builder.CreateVector(rlimits_vec) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::SyscallRule>> syscall_rules_vec;
        for (const auto& rule : obj.seccomp.syscalls) {
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
                        builder.CreateVectorOfStrings(obj.seccomp.archs),
                        builder.CreateVectorOfStrings(obj.seccomp.flags),
                        builder.CreateString(obj.seccomp.default_action),
                        obj.seccomp.default_errno) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::Device>> devices_vec;
        devices_vec.reserve(obj.devices.size());
        for (const auto& dev : obj.devices) {
                devices_vec.push_back(FB::OCIRuntime::CreateDevice(
                                        builder,
                                        builder.CreateString(dev.host_path.string()),
                                        builder.CreateString(dev.container_path.string())));
        }
        auto devices_off { builder.CreateVector(devices_vec) };
        auto tcp_off     { builder.CreateVectorOfStrings(obj.networks.tcp_ports) };
        auto udp_off     { builder.CreateVectorOfStrings(obj.networks.udp_ports) };
        auto networks_off { FB::OCIRuntime::CreateNetwork(
                        builder, tcp_off, udp_off, obj.networks.auto_tcp, obj.networks.auto_udp) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::TimeOffset>> timeoff_vec;
        timeoff_vec.reserve(obj.timeoffsets.size());
        for (const auto& to : obj.timeoffsets) {
                timeoff_vec.push_back(FB::OCIRuntime::CreateTimeOffset(
                                        builder, builder.CreateString(to.type), to.secs, to.nanosecs));
        }
        auto timeoffsets_off { builder.CreateVector(timeoff_vec) };
        std::vector<flatbuffers::Offset<FB::OCIRuntime::Namespace>> ns_vec;
        ns_vec.reserve(obj.namespaces.size());
        for (const auto& ns : obj.namespaces) {
                ns_vec.push_back(FB::OCIRuntime::CreateNamespace(
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
        fb_builder.add_pty_slave_fd(obj.pty_slave_fd);
        fb_builder.add_control_sock(obj.control_sock);
        fb_builder.add_pid(obj.pid);
        fb_builder.add_net_pid(obj.net_pid);
        fb_builder.add_vfs(obj.vfs);
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
        return fb_builder.Finish();
}
auto Serialization::deserialize(const FB::ContainerConfig* fb) -> ContainerConfig {
        ContainerConfig obj{};
        if (!fb) return obj;
        obj.pty_slave_fd  = fb->pty_slave_fd();
        obj.control_sock  = fb->control_sock();
        obj.pid           = fb->pid();
        obj.net_pid       = fb->net_pid();
        obj.vfs           = fb->vfs();
        if (fb->container_id())   obj.container_id   = fb->container_id()->str();
        if (fb->hostname())       obj.hostname        = fb->hostname()->str();
        if (fb->domain_name())    obj.domain_name     = fb->domain_name()->str();
        if (fb->pty_slave_name()) obj.pty_slave_name  = fb->pty_slave_name()->str();
        if (fb->cgroups_path())   obj.cgroups_path    = fb->cgroups_path()->str();
        if (const auto* r = fb->rootfs()) {
                if (r->path()) obj.rootfs.path = r->path()->str();
                obj.rootfs.read_only = r->read_only();
        }
        if (const auto* rp = fb->rootfs_propagation()) {
                if (rp->type()) obj.rootfs_propagation.type = rp->type()->str();
        }
        if (const auto* t = fb->terminal()) obj.terminal.value         = t->value();
        if (const auto* d = fb->detach())   obj.detach.value           = d->value();
        if (const auto* c = fb->console_size()) {
                obj.console_size.height = c->height();
                obj.console_size.width  = c->width();
        }
        if (const auto* u = fb->user()) {
                obj.user.uid   = u->uid();
                obj.user.gid   = u->gid();
                obj.user.umask = u->umask();
                if (u->additional_gids()) {
                        obj.user.additional_gids.assign(
                                        u->additional_gids()->begin(), u->additional_gids()->end());
                }
        }
        if (const auto* um = fb->uid_mapping()) {
                obj.uid_mapping.container_id = um->container_id();
                obj.uid_mapping.host_id      = um->host_id();
                obj.uid_mapping.size         = um->size();
        }
        if (const auto* gm = fb->gid_mapping()) {
                obj.gid_mapping.container_id = gm->container_id();
                obj.gid_mapping.host_id      = gm->host_id();
                obj.gid_mapping.size         = gm->size();
        }
        if (const auto* e = fb->env()) {
                if (e->value()) {
                        for (const auto* s : *e->value()) obj.env.value.push_back(s->str());
                }
        }
        if (const auto* c = fb->cwd()) {
                if (c->value()) obj.cwd.value = c->value()->str();
        }
        if (const auto* a = fb->args()) {
                if (a->value()) {
                        for (const auto* s : *a->value()) obj.args.value.push_back(s->str());
                }
        }
        if (const auto* o = fb->oom_score())         obj.oom_score.value         = o->value();
        if (const auto* n = fb->no_new_privileges()) obj.no_new_privileges.value = n->value();
        if (const auto* s = fb->schedular_opts()) {
                if (s->flags())  for (const auto* f : *s->flags())  obj.schedular_opts.flags.push_back(f->str());
                if (s->policy()) obj.schedular_opts.policy   = s->policy()->str();
                obj.schedular_opts.runtime  = s->runtime();
                obj.schedular_opts.deadline = s->deadline();
                obj.schedular_opts.period   = s->period();
                obj.schedular_opts.nice     = s->nice();
                obj.schedular_opts.priority = s->priority();
        }
        if (const auto* c = fb->capabilities()) {
                auto copy = [](const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* v,
                                std::vector<std::string>& out) {
                        if (v) for (const auto* s : *v) out.push_back(s->str());
                };
                copy(c->bounding(),    obj.capabilities.bounding);
                copy(c->effective(),   obj.capabilities.effective);
                copy(c->inheritable(), obj.capabilities.inheritable);
                copy(c->permitted(),   obj.capabilities.permitted);
                copy(c->ambient(),     obj.capabilities.ambient);
        }
        if (fb->rlimits()) {
                for (const auto* rl : *fb->rlimits()) {
                        OCIRuntime::RLimit r{};
                        if (rl->name()) r.name = rl->name()->str();
                        r.hard_limit = rl->hard_limit();
                        r.soft_limit = rl->soft_limit();
                        obj.rlimits.push_back(std::move(r));
                }
        }
        if (const auto* sc = fb->seccomp()) {
                if (sc->archs())          for (const auto* s : *sc->archs())  obj.seccomp.archs.push_back(s->str());
                if (sc->flags())          for (const auto* s : *sc->flags())  obj.seccomp.flags.push_back(s->str());
                if (sc->default_action()) obj.seccomp.default_action = sc->default_action()->str();
                obj.seccomp.default_errno = sc->default_errno();
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
                                obj.seccomp.syscalls.push_back(std::move(sr));
                        }
                }
        }
        if (fb->devices()) {
                for (const auto* dev : *fb->devices()) {
                        OCIRuntime::Device d{};
                        if (dev->host_path())      d.host_path      = dev->host_path()->str();
                        if (dev->container_path()) d.container_path = dev->container_path()->str();
                        obj.devices.push_back(std::move(d));
                }
        }
        if (const auto* net = fb->networks()) {
                if (net->tcp_ports()) for (const auto* p : *net->tcp_ports()) obj.networks.tcp_ports.push_back(p->str());
                if (net->udp_ports()) for (const auto* p : *net->udp_ports()) obj.networks.udp_ports.push_back(p->str());
                obj.networks.auto_tcp = net->auto_tcp();
                obj.networks.auto_udp = net->auto_udp();
        }
        if (fb->timeoffsets()) {
                for (const auto* to : *fb->timeoffsets()) {
                        OCIRuntime::TimeOffset t{};
                        if (to->type()) t.type = to->type()->str();
                        t.secs    = to->secs();
                        t.nanosecs = to->nanosecs();
                        obj.timeoffsets.push_back(std::move(t));
                }
        }
        if (fb->namespaces()) {
                for (const auto* ns : *fb->namespaces()) {
                        OCIRuntime::Namespace n{};
                        if (ns->path()) n.path = ns->path()->str();
                        if (ns->type()) n.type = ns->type()->str();
                        obj.namespaces.push_back(std::move(n));
                }
        }
        if (fb->mounts()) {
                for (const auto* mnt : *fb->mounts()) {
                        OCIRuntime::Mount m{};
                        if (mnt->options())     for (const auto* s : *mnt->options()) m.options.push_back(s->str());
                        if (mnt->flags())       for (const auto* s : *mnt->flags())   m.flags.push_back(s->str());
                        if (mnt->attrs())       for (const auto* s : *mnt->attrs())   m.attrs.push_back(s->str());
                        if (mnt->destination()) m.destination = mnt->destination()->str();
                        if (mnt->type())        m.type        = mnt->type()->str();
                        if (mnt->source())      m.source      = mnt->source()->str();
                        obj.mounts.push_back(std::move(m));
                }
        }
        if (fb->masked_paths() && fb->masked_paths()->paths()) {
                for (const auto* p : *fb->masked_paths()->paths())
                        obj.masked_paths.paths.push_back(p->str());
        }
        if (fb->read_only_paths() && fb->read_only_paths()->paths()) {
                for (const auto* p : *fb->read_only_paths()->paths())
                        obj.read_only_paths.paths.push_back(p->str());
        }
        return obj;
}
auto Serialization::serialize(flatbuffers::FlatBufferBuilder& builder, const ImageMetadata& obj) -> flatbuffers::Offset<FB::ImageMetadata> {
        auto id_off           { builder.CreateString(obj.id) };
        auto name_off         { builder.CreateString(obj.name) };
        auto tag_off          { builder.CreateString(obj.tag) };
        auto digest_off       { builder.CreateString(obj.digest) };
        auto path_off         { builder.CreateString(obj.path) };
        auto architecture_off { builder.CreateString(obj.architecture) };
        auto source_off       { builder.CreateString(obj.source) };
        FB::ImageMetadataBuilder fb_builder { builder };
        fb_builder.add_id(id_off);
        fb_builder.add_name(name_off);
        fb_builder.add_tag(tag_off);
        fb_builder.add_digest(digest_off);
        fb_builder.add_path(path_off);
        fb_builder.add_size_bytes(obj.size_bytes);
        fb_builder.add_created_at(obj.created_at);
        fb_builder.add_architecture(architecture_off);
        fb_builder.add_source(source_off);
        return fb_builder.Finish();
}
auto Serialization::deserialize(const FB::ImageMetadata* fb) -> ImageMetadata {
        ImageMetadata obj{};
        if (!fb) return obj;
        if (fb->id())           obj.id           = fb->id()->str();
        if (fb->name())         obj.name         = fb->name()->str();
        if (fb->tag())          obj.tag          = fb->tag()->str();
        if (fb->digest())       obj.digest       = fb->digest()->str();
        if (fb->path())         obj.path         = fb->path()->str();
        if (fb->architecture()) obj.architecture = fb->architecture()->str();
        if (fb->source())       obj.source       = fb->source()->str();
        obj.size_bytes = fb->size_bytes();
        obj.created_at = fb->created_at();
        return obj;
}
