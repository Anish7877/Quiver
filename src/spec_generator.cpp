#include "spec_generator.hpp"
#include <algorithm>
#include <format>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

namespace SpecGenerator {

// Maximum characters taken from container_id for use as the UTS hostname.
// Mirrors the Docker/Podman convention of using the first 12 hex chars of the container ID.
static constexpr std::size_t HOSTNAME_MAX_LEN{12};

// ── Seccomp helpers ──────────────────────────────────────────────────────────
//
// The seccomp profile below is modelled on the OCI 1.2.1 default profile used
// by Podman/crun for rootless containers. The design follows a default-deny
// posture (SCMP_ACT_ERRNO / ENOSYS = 38) with an explicit large allowlist,
// plus targeted per-syscall overrides for conditional and dangerous calls.
//
// Rule evaluation order in libseccomp: rules are evaluated first-match-wins
// per syscall. Within a single SyscallRule, ALL arg constraints must match
// (logical AND). Multiple SyscallRules for the same syscall are evaluated in
// the order they appear in the syscalls array.
//
// Arg struct field order: { op, value, value_two, index }
// where 'index' is the zero-based syscall argument position.

// Build a simple deny rule: all listed syscalls → SCMP_ACT_ERRNO(errno_ret),
// no argument constraints.
[[nodiscard]] static auto make_deny_rule(
        std::vector<std::string> names,
        std::uint32_t            errno_ret) -> OCIRuntime::Seccomp::SyscallRule {
        return OCIRuntime::Seccomp::SyscallRule{
                {},                 // args — no constraints
                std::move(names),
                "SCMP_ACT_ERRNO",
                errno_ret
        };
}

// Build a simple allow rule: all listed syscalls → SCMP_ACT_ALLOW,
// no argument constraints.
[[nodiscard]] static auto make_allow_rule(
        std::vector<std::string> names) -> OCIRuntime::Seccomp::SyscallRule {
        return OCIRuntime::Seccomp::SyscallRule{
                {},
                std::move(names),
                "SCMP_ACT_ALLOW",
                0
        };
}

// Build a conditional allow rule: single syscall, one or more argument constraints.
[[nodiscard]] static auto make_conditional_allow(
        std::string                             name,
        std::vector<OCIRuntime::Seccomp::Arg>  args) -> OCIRuntime::Seccomp::SyscallRule {
        return OCIRuntime::Seccomp::SyscallRule{
                std::move(args),
                {std::move(name)},
                "SCMP_ACT_ALLOW",
                0
        };
}

// Build a conditional deny rule: single syscall, one or more argument constraints.
[[nodiscard]] static auto make_conditional_deny(
        std::string                             name,
        std::vector<OCIRuntime::Seccomp::Arg>  args,
        std::uint32_t                           errno_ret) -> OCIRuntime::Seccomp::SyscallRule {
        return OCIRuntime::Seccomp::SyscallRule{
                std::move(args),
                {std::move(name)},
                "SCMP_ACT_ERRNO",
                errno_ret
        };
}

// ── Seccomp profile builder ──────────────────────────────────────────────────
[[nodiscard]] static auto build_seccomp_profile() -> OCIRuntime::Seccomp {

        // Arg shorthand: { op, value, value_two=0, index }
        // value_two is only meaningful for SCMP_CMP_MASKED_EQ.
        auto arg{[](std::string op, std::uint64_t value, std::uint32_t index) {
                return OCIRuntime::Seccomp::Arg{std::move(op), value, 0, index};
        }};

        std::vector<OCIRuntime::Seccomp::SyscallRule> rules{};

        // ── Rule 1: Unconditional deny — dangerous or obsolete syscalls ──────
        // These are blocked with EPERM (1). They have no legitimate use in a
        // standard unprivileged container: they interact with raw hardware,
        // bypass namespaces, or are deprecated kernel interfaces.
        rules.emplace_back(make_deny_rule({
                "bdflush",              // obsolete buffer flush — removed in Linux 5.5
                "cachestat",            // cache statistics — information leak risk
                "futex_requeue",        // futex variants — covered by allowlist futex
                "futex_wait",
                "futex_waitv",
                "futex_wake",
                "io_pgetevents",        // AIO with signal — covered by io_* allowlist
                "io_pgetevents_time64",
                "kexec_file_load",      // kernel exec — absolute privilege escalation
                "kexec_load",
                "map_shadow_stack",     // shadow stack management — not needed in containers
                "migrate_pages",        // NUMA page migration — host resource manipulation
                "move_pages",
                "nfsservctl",           // NFS server control — removed in Linux 3.1
                "nice",                 // obsolete — use setpriority
                "oldfstat",             // obsolete stat variants
                "oldlstat",
                "oldolduname",
                "oldstat",
                "olduname",
                "pciconfig_iobase",     // PCI hardware access
                "pciconfig_read",
                "pciconfig_write",
                "sgetmask",             // obsolete signal mask
                "ssetmask",
                "swapoff",              // swap management — host kernel resource
                "swapon",
                "syscall",              // indirect syscall — arch-specific, abuse vector
                "sysfs",                // obsolete filesystem info
                "uselib",               // obsolete shared library loading
                "userfaultfd",          // userspace page fault handling — TOCTOU risk
                "ustat",                // obsolete — use statfs
                "vm86",                 // x86 virtual 8086 mode — not meaningful in container
                "vm86old",
                "vmsplice"              // pipe to memory splice — potential info leak
        }, 1));

        // ── Rule 2: Unconditional allow — standard POSIX/Linux operations ────
        // This is the core allowlist. Every syscall here is needed for normal
        // process operation, file I/O, networking, signals, or memory management.
        // Sourced from the OCI 1.2.1 Podman/crun default profile.
        rules.emplace_back(make_allow_rule({
                "_llseek",              // 32-bit lseek compat
                "_newselect",           // old select compat
                "accept",
                "accept4",
                "access",
                "adjtimex",             // read-only time info in user ns
                "alarm",
                "bind",
                "brk",
                "capget",
                "capset",
                "chdir",
                "chmod",
                "chown",
                "chown32",
                "clock_adjtime",        // allowed — user ns clock is isolated
                "clock_adjtime64",
                "clock_getres",
                "clock_getres_time64",
                "clock_gettime",
                "clock_gettime64",
                "clock_nanosleep",
                "clock_nanosleep_time64",
                "clone",
                "clone3",
                "close",
                "close_range",
                "connect",
                "copy_file_range",
                "creat",
                "dup",
                "dup2",
                "dup3",
                "epoll_create",
                "epoll_create1",
                "epoll_ctl",
                "epoll_ctl_old",
                "epoll_pwait",
                "epoll_pwait2",
                "epoll_wait",
                "epoll_wait_old",
                "eventfd",
                "eventfd2",
                "execve",
                "execveat",
                "exit",
                "exit_group",
                "faccessat",
                "faccessat2",
                "fadvise64",
                "fadvise64_64",
                "fallocate",
                "fanotify_init",
                "fanotify_mark",
                "fchdir",
                "fchmod",
                "fchmodat",
                "fchmodat2",
                "fchown",
                "fchown32",
                "fchownat",
                "fcntl",
                "fcntl64",
                "fdatasync",
                "fgetxattr",
                "flistxattr",
                "flock",
                "fork",
                "fremovexattr",
                "fsconfig",
                "fsetxattr",
                "fsmount",
                "fsopen",
                "fspick",
                "fstat",
                "fstat64",
                "fstatat64",
                "fstatfs",
                "fstatfs64",
                "fsync",
                "ftruncate",
                "ftruncate64",
                "futex",
                "futex_time64",
                "futimesat",
                "get_mempolicy",
                "get_robust_list",
                "get_thread_area",
                "getcpu",
                "getcwd",
                "getdents",
                "getdents64",
                "getegid",
                "getegid32",
                "geteuid",
                "geteuid32",
                "getgid",
                "getgid32",
                "getgroups",
                "getgroups32",
                "getitimer",
                "getpeername",
                "getpgid",
                "getpgrp",
                "getpid",
                "getppid",
                "getpriority",
                "getrandom",
                "getresgid",
                "getresgid32",
                "getresuid",
                "getresuid32",
                "getrlimit",
                "getrusage",
                "getsid",
                "getsockname",
                "getsockopt",
                "gettid",
                "gettimeofday",
                "getuid",
                "getuid32",
                "getxattr",
                "inotify_add_watch",
                "inotify_init",
                "inotify_init1",
                "inotify_rm_watch",
                "io_cancel",
                "io_destroy",
                "io_getevents",
                "io_setup",
                "io_submit",
                "ioctl",
                "ioprio_get",
                "ioprio_set",
                "ipc",
                "keyctl",               // allowed — container has its own key namespace
                "kill",
                "landlock_add_rule",
                "landlock_create_ruleset",
                "landlock_restrict_self",
                "lchown",
                "lchown32",
                "lgetxattr",
                "link",
                "linkat",
                "listen",
                "listxattr",
                "llistxattr",
                "lremovexattr",
                "lseek",
                "lsetxattr",
                "lstat",
                "lstat64",
                "madvise",
                "mbind",
                "membarrier",
                "memfd_create",
                "memfd_secret",
                "mincore",
                "mkdir",
                "mkdirat",
                "mknod",
                "mknodat",
                "mlock",
                "mlock2",
                "mlockall",
                "mmap",
                "mmap2",
                "mount",                // allowed — container has its own mount namespace
                "mount_setattr",
                "move_mount",
                "mprotect",
                "mq_getsetattr",
                "mq_notify",
                "mq_open",
                "mq_timedreceive",
                "mq_timedreceive_time64",
                "mq_timedsend",
                "mq_timedsend_time64",
                "mq_unlink",
                "mremap",
                "msgctl",
                "msgget",
                "msgrcv",
                "msgsnd",
                "msync",
                "munlock",
                "munlockall",
                "munmap",
                "name_to_handle_at",
                "nanosleep",
                "newfstatat",
                "open",
                "open_tree",
                "openat",
                "openat2",
                "pause",
                "pidfd_getfd",
                "pidfd_open",
                "pidfd_send_signal",
                "pipe",
                "pipe2",
                "pivot_root",           // allowed — ContainerRuntime uses this
                "pkey_alloc",
                "pkey_free",
                "pkey_mprotect",
                "poll",
                "ppoll",
                "ppoll_time64",
                "prctl",
                "pread64",
                "preadv",
                "preadv2",
                "prlimit64",
                "process_mrelease",
                "process_vm_readv",
                "process_vm_writev",
                "pselect6",
                "pselect6_time64",
                "ptrace",
                "pwrite64",
                "pwritev",
                "pwritev2",
                "read",
                "readahead",
                "readlink",
                "readlinkat",
                "readv",
                "reboot",               // allowed inside user+pid namespace (no-op on host)
                "recv",
                "recvfrom",
                "recvmmsg",
                "recvmmsg_time64",
                "recvmsg",
                "remap_file_pages",
                "removexattr",
                "rename",
                "renameat",
                "renameat2",
                "restart_syscall",
                "rmdir",
                "rseq",
                "rt_sigaction",
                "rt_sigpending",
                "rt_sigprocmask",
                "rt_sigqueueinfo",
                "rt_sigreturn",
                "rt_sigsuspend",
                "rt_sigtimedwait",
                "rt_sigtimedwait_time64",
                "rt_tgsigqueueinfo",
                "sched_get_priority_max",
                "sched_get_priority_min",
                "sched_getaffinity",
                "sched_getattr",
                "sched_getparam",
                "sched_getscheduler",
                "sched_rr_get_interval",
                "sched_rr_get_interval_time64",
                "sched_setaffinity",
                "sched_setattr",
                "sched_setparam",
                "sched_setscheduler",
                "sched_yield",
                "seccomp",
                "select",
                "semctl",
                "semget",
                "semop",
                "semtimedop",
                "semtimedop_time64",
                "send",
                "sendfile",
                "sendfile64",
                "sendmmsg",
                "sendmsg",
                "sendto",
                "set_mempolicy",
                "set_robust_list",
                "set_thread_area",
                "set_tid_address",
                "setfsgid",
                "setfsgid32",
                "setfsuid",
                "setfsuid32",
                "setgid",
                "setgid32",
                "setgroups",
                "setgroups32",
                "setitimer",
                "setpgid",
                "setpriority",
                "setregid",
                "setregid32",
                "setresgid",
                "setresgid32",
                "setresuid",
                "setresuid32",
                "setreuid",
                "setreuid32",
                "setrlimit",
                "setsid",
                "setsockopt",
                "setuid",
                "setuid32",
                "setxattr",
                "shmat",
                "shmctl",
                "shmdt",
                "shmget",
                "shutdown",
                "sigaltstack",
                "signal",
                "signalfd",
                "signalfd4",
                "sigprocmask",
                "sigreturn",
                "socketcall",
                "socketpair",
                "splice",
                "stat",
                "stat64",
                "statfs",
                "statfs64",
                "statx",
                "symlink",
                "symlinkat",
                "sync",
                "sync_file_range",
                "syncfs",
                "sysinfo",
                "syslog",
                "tee",
                "tgkill",
                "time",
                "timer_create",
                "timer_delete",
                "timer_getoverrun",
                "timer_gettime",
                "timer_gettime64",
                "timer_settime",
                "timer_settime64",
                "timerfd_create",
                "timerfd_gettime",
                "timerfd_gettime64",
                "timerfd_settime",
                "timerfd_settime64",
                "times",
                "tkill",
                "truncate",
                "truncate64",
                "ugetrlimit",
                "umask",
                "umount",
                "umount2",
                "uname",
                "unlink",
                "unlinkat",
                "unshare",
                "utime",
                "utimensat",
                "utimensat_time64",
                "utimes",
                "vfork",
                "wait4",
                "waitid",
                "waitpid",
                "write",
                "writev"
        }));

        // ── Rule 3: personality — allow specific known-safe ABI personas ──────
        // personality() controls the execution domain (32-bit compat, ABI quirks).
        // Only the five values used by Linux/glibc are permitted.
        // 0x00000000 = PER_LINUX        (native)
        // 0x00000008 = PER_LINUX_32BIT  (address limit quirk)
        // 0x00020000 = PER_LINUX32      (32-bit compat mode)
        // 0x00020008 = PER_LINUX32 | PER_ADDR_COMPAT_LAYOUT
        // 0xFFFFFFFF = PER_QUERY        (query current personality, no change)
        rules.emplace_back(make_conditional_allow(
                "personality",
                {arg("SCMP_CMP_EQ", 0x00000000, 0)}));
        rules.emplace_back(make_conditional_allow(
                "personality",
                {arg("SCMP_CMP_EQ", 0x00000008, 0)}));
        rules.emplace_back(make_conditional_allow(
                "personality",
                {arg("SCMP_CMP_EQ", 0x00020000, 0)}));   // PER_LINUX32
        rules.emplace_back(make_conditional_allow(
                "personality",
                {arg("SCMP_CMP_EQ", 0x00020008, 0)}));   // PER_LINUX32 | PER_ADDR_COMPAT_LAYOUT
        rules.emplace_back(make_conditional_allow(
                "personality",
                {arg("SCMP_CMP_EQ", 0xFFFFFFFF, 0)}));   // PER_QUERY

        // ── Rule 4: arch_prctl — x86_64 specific, always allow ───────────────
        // Required for thread-local storage setup (FS/GS base registers).
        // Not present on other architectures — libseccomp silently ignores
        // unknown syscall names on non-matching architectures.
        rules.emplace_back(make_allow_rule({"arch_prctl"}));

        // ── Rule 5: modify_ldt — x86/x86_64 LDT access ───────────────────────
        // Needed for Wine and some legacy 32-bit code. Low risk in a user ns.
        rules.emplace_back(make_allow_rule({"modify_ldt"}));

        // ── Rule 6: chroot — allow ────────────────────────────────────────────
        // ContainerRuntime::jail_process() does not use chroot (uses pivot_root),
        // but processes inside the container may legitimately call it.
        rules.emplace_back(make_allow_rule({"chroot"}));

        // ── Rule 7: open_by_handle_at — deny ─────────────────────────────────
        // Bypasses chroot/bind-mount boundaries via persistent file handles.
        // This is a known container escape vector.
        rules.emplace_back(make_deny_rule({"open_by_handle_at"}, 1));

        // ── Rule 8: host-interaction syscalls — deny ──────────────────────────
        // These interact with host kernel state even from within a user namespace:
        //   lookup_dcookie  : kernel profiling infrastructure
        //   perf_event_open : hardware performance counters — side-channel risk
        //   quotactl        : filesystem quota management
        //   quotactl_fd     : same via fd
        //   setdomainname   : changes UTS domain name — blocked post-exec;
        //                     ContainerRuntime calls setdomainname() BEFORE
        //                     seccomp is applied, so this only affects children.
        //   sethostname     : same rationale as setdomainname
        //   setns           : joining existing namespaces from inside — escape vector
        rules.emplace_back(make_deny_rule({
                "lookup_dcookie",
                "perf_event_open",
                "quotactl",
                "quotactl_fd",
                "setdomainname",
                "sethostname",
                "setns"
        }, 1));

        // ── Rule 9: kernel module loading — deny ──────────────────────────────
        rules.emplace_back(make_deny_rule({
                "delete_module",
                "finit_module",
                "init_module",
                "query_module"
        }, 1));

        // ── Rule 10: process accounting — deny ────────────────────────────────
        rules.emplace_back(make_deny_rule({"acct"}, 1));

        // ── Rule 11: cross-process inspection — deny ──────────────────────────
        //   kcmp           : compares kernel resources across processes
        //   process_madvise: advise on another process's memory — information leak
        rules.emplace_back(make_deny_rule({"kcmp", "process_madvise"}, 1));

        // ── Rule 12: direct hardware port I/O — deny ─────────────────────────
        rules.emplace_back(make_deny_rule({"ioperm", "iopl"}, 1));

        // ── Rule 13: host clock modification — deny ───────────────────────────
        // Time namespace provides an isolated clock view; direct modification
        // of the system clock from a container is not permitted.
        rules.emplace_back(make_deny_rule({
                "clock_settime",
                "clock_settime64",
                "settimeofday",
                "stime"
        }, 1));

        // ── Rule 14: vhangup — deny ───────────────────────────────────────────
        // Hangs up the current terminal — can disrupt host session.
        rules.emplace_back(make_deny_rule({"vhangup"}, 1));

        // ── Rule 15: socket — conditional allow/deny ─────────────────────────
        // Arg layout: { op, value, value_two=0, index }
        //   index 0 = domain (address family)
        //   index 2 = protocol
        //
        // AF_BLUETOOTH (31) — deny entirely: Bluetooth sockets bypass network ns
        rules.emplace_back(make_conditional_deny(
                "socket",
                {arg("SCMP_CMP_EQ", 31, 0)},   // domain == AF_BLUETOOTH
                1));

        // AF_NETLINK (16) + NETLINK_ROUTE (9) — deny: raw netlink routing socket
        // can manipulate host routes even from a network namespace.
        rules.emplace_back(make_conditional_deny(
                "socket",
                {
                        arg("SCMP_CMP_EQ", 16, 0),   // domain == AF_NETLINK
                        arg("SCMP_CMP_EQ",  9, 2)    // protocol == NETLINK_ROUTE
                },
                22));   // EINVAL — matches Podman behaviour

        // AF_NETLINK (16) + protocol != NETLINK_ROUTE — allow (e.g. NETLINK_AUDIT)
        rules.emplace_back(make_conditional_allow(
                "socket",
                {
                        arg("SCMP_CMP_EQ", 16, 0),   // domain == AF_NETLINK
                        arg("SCMP_CMP_NE",  9, 2)    // protocol != NETLINK_ROUTE
                }));

        // All other address families — allow
        rules.emplace_back(make_conditional_allow(
                "socket",
                {arg("SCMP_CMP_NE", 16, 0)}));   // domain != AF_NETLINK

        // ── Rule 16: bpf — deny ───────────────────────────────────────────────
        // BPF program loading can be used to probe kernel memory and bypass
        // seccomp filters themselves.
        rules.emplace_back(make_deny_rule({"bpf"}, 1));

        // ── Rule 17: perf_event_open — deny (explicit, belt-and-suspenders) ───
        // Already in Rule 8, but Podman emits it twice. Harmless duplication;
        // libseccomp merges identical rules.
        rules.emplace_back(make_deny_rule({"perf_event_open"}, 1));

        return OCIRuntime::Seccomp{
                std::move(rules),
                {"x86_64", "x86", "x32"},   // SCMP_ARCH_X86_64, SCMP_ARCH_X86, SCMP_ARCH_X32
                {},                          // flags — none needed for default profile
                "SCMP_ACT_ERRNO",            // default: deny unknown syscalls
                38                           // ENOSYS — "Function not implemented"
        };
}

// ── Main entry point ─────────────────────────────────────────────────────────

[[nodiscard]] auto generate_default_rootless_spec(
        const std::string& container_id,
        const std::string& rootfs_path) -> ContainerConfig {

        // ── Input validation ─────────────────────────────────────────────────
        if (container_id.empty()) [[unlikely]] {
                throw std::runtime_error(
                        "Spec Generator Error: container_id cannot be empty.");
        }
        if (rootfs_path.empty()) [[unlikely]] {
                throw std::runtime_error(
                        "Spec Generator Error: rootfs_path cannot be empty.");
        }
        if (!fs::exists(rootfs_path)) [[unlikely]] {
                throw std::runtime_error(std::format(
                        "Spec Generator Error: rootfs path does not exist -> '{}'.",
                        rootfs_path));
        }
        if (!fs::is_directory(rootfs_path)) [[unlikely]] {
                throw std::runtime_error(std::format(
                        "Spec Generator Error: rootfs path is not a directory -> '{}'.",
                        rootfs_path));
        }

        ContainerConfig spec{};

        // ── Identity ─────────────────────────────────────────────────────────
        spec.container_id = container_id;

        // UTS hostname: first HOSTNAME_MAX_LEN characters of the container ID,
        // matching the Docker/Podman convention.
        spec.hostname = container_id.substr(
                0, std::min(container_id.size(), HOSTNAME_MAX_LEN));

        // NIS domain name: "local" is a safe, mDNS-compatible default.
        // ContainerRuntime calls setdomainname() before seccomp is applied,
        // so the seccomp deny on setdomainname only affects container children.
        spec.domain_name = "local";

        // ── Execution context ────────────────────────────────────────────────
        spec.cwd  = OCIRuntime::Cwd{"/"};
        spec.env  = OCIRuntime::Env{{
                "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
                "container=quiver",
                "TERM=xterm",
                "HOME=/root",
                std::format("HOSTNAME={}", spec.hostname)
        }};

        // terminal=true: ContainerMonitor opens a PTY and forwards it to the
        // container's stdin/stdout/stderr via the control socket.
        spec.terminal = OCIRuntime::Terminal{false};

        // detach=false: invoke_container() blocks in the calling process until
        // the container exits. Set true for daemon-mode containers.
        spec.detach = OCIRuntime::Detach{false};

        // Query the calling terminal's dimensions; fall back to 80×24 if
        // stdout is not a TTY (e.g. when called from a non-interactive context).
        struct winsize ws{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0
                        && ws.ws_row > 0
                        && ws.ws_col > 0) {
                spec.console_size = OCIRuntime::ConsoleSize{ws.ws_row, ws.ws_col};
        }
        else {
                spec.console_size = OCIRuntime::ConsoleSize{24, 80};
        }

        // ── Security & process flags ──────────────────────────────────────────
        // no_new_privileges prevents execve from granting additional capabilities
        // via setuid binaries or file capabilities inside the container.
        spec.no_new_privileges = OCIRuntime::NoNewPrivileges{true};

        // OOM score adjustment: 0 = kernel default. Increase (up to 1000) to
        // make the container a preferred OOM kill target.
        spec.oom_score = OCIRuntime::OomScoreAdj{0};

        // Scheduler: empty policy causes Schedular::apply_opts() to return early
        // without changing the container's scheduling policy.
        spec.schedular_opts = OCIRuntime::SchedularOpts{};

        // ── Root filesystem ──────────────────────────────────────────────────
        // vfs=false: use OverlayFS (copy-on-write). The merged directory is
        // created under ~/.quiver/filesystems/quiver_{id}/.
        // Set vfs=true to fall back to a full VFS copy of the image layers.
        spec.rootfs = OCIRuntime::Root{fs::path(rootfs_path), false};
        spec.vfs    = false;

        // Rootfs propagation: "private" ensures mount events inside the container
        // do not propagate to the host mount namespace. "slave" would allow the
        // host's mount events to propagate in (useful for bind-mount scenarios).
        spec.rootfs_propagation = OCIRuntime::RootfsPropagation{"private"};

        // cgroups_path is intentionally left empty.
        // CGroupsManagerCreator::create_cgroups_manager() resolves the backend:
        //   - empty path + systemd present  → SystemdCGroupsManager
        //   - empty path + no systemd       → RawCGroupsManager (throws if
        //                                     delegated_path does not exist)
        // On non-systemd hosts the caller MUST set this to a delegated cgroup
        // path that the user owns (e.g. /sys/fs/cgroup/user.slice/<uid>/<scope>).
        spec.cgroups_path = "";

        // ── User namespace mapping ────────────────────────────────────────────
        // Inside the container the process appears to run as root (uid=0, gid=0).
        // The user namespace maps container root to the invoking host user so no
        // real root privileges are obtained on the host.
        spec.user = OCIRuntime::User{
                0,      // uid — root inside container
                0,      // gid
                022,    // umask — octal 022 = decimal 18 (matches config.json)
                {}      // additional_gids
        };
        spec.uid_mapping = OCIRuntime::UidMapping{
                0,                                             // container_id
                static_cast<std::uint32_t>(getuid()),          // host_id
                65536                                          // size
        };
        spec.gid_mapping = OCIRuntime::GidMapping{
                0,
                static_cast<std::uint32_t>(getgid()),
                65536
        };

        // ── Linux namespaces ─────────────────────────────────────────────────
        // Empty path → create a new namespace of that type (unshare path).
        // Non-empty path → join an existing namespace via setns (used for
        // shared network namespaces between containers, e.g. pod networking).
        //
        // "cgroup" namespace is included so the container has an isolated view
        // of the cgroups hierarchy (requires kernel ≥ 4.6).
        spec.namespaces = {
                OCIRuntime::Namespace{"", "pid"},
                OCIRuntime::Namespace{"", "network"},
                OCIRuntime::Namespace{"", "ipc"},
                OCIRuntime::Namespace{"", "uts"},
                OCIRuntime::Namespace{"", "mount"},
                OCIRuntime::Namespace{"", "cgroup"}
        };

        // ── Capabilities ──────────────────────────────────────────────────────
        // Minimal set matching the OCI 1.2.1 Podman default for rootless
        // unprivileged containers. CAP_SETFCAP is needed to set file capabilities
        // during package installation inside the container.
        //
        // Inheritable and ambient sets are empty:
        //   - inheritable: caps are not inherited across execve by default
        //   - ambient: raising ambient requires the cap in both permitted AND
        //     inheritable; empty inheritable makes ambient raises impossible,
        //     which is the safe default
        std::vector<std::string> default_caps{
                "CAP_CHOWN",
                "CAP_DAC_OVERRIDE",
                "CAP_FOWNER",
                "CAP_FSETID",
                "CAP_KILL",
                "CAP_NET_BIND_SERVICE",
                "CAP_SETFCAP",
                "CAP_SETGID",
                "CAP_SETPCAP",
                "CAP_SETUID",
                "CAP_SYS_CHROOT"
        };
        spec.capabilities = OCIRuntime::Capabilities{
                default_caps, // bounding  — hard ceiling for all caps in this container
                default_caps, // effective — currently active caps
                {},           // inheritable
                default_caps, // permitted — superset of effective
                {}            // ambient
        };

        // ── Resource limits ───────────────────────────────────────────────────
        // Names must match the keys in OCIRuntime::RLIMIT_STR_MAP (no RLIMIT_ prefix).
        // hard ≥ soft is enforced by the kernel.
        spec.rlimits = {
                // RLIMIT_NOFILE: max open file descriptors.
                // 524288 matches the Podman default for containers.
                OCIRuntime::RLimit{"nofile", 524288, 524288},
                // RLIMIT_NPROC: max processes/threads for the container's uid
                // inside the user namespace.
                OCIRuntime::RLimit{"nproc", 60878, 60878}
        };

        // ── Mounts ────────────────────────────────────────────────────────────
        // proc, sys, dev, dev/pts, dev/shm, tmp, and run are mounted
        // unconditionally by ContainerRuntime::mount_necessary_dirs() and must
        // NOT be listed here to avoid double-mount failures.
        //
        // Mount field order: { options, flags, attrs, destination, type, source }
        //   options → passed as data string to mount(2) (filesystem-specific)
        //   flags   → looked up in MOUNT_FLAGS_STR_MAP → MS_* constants
        //   attrs   → looked up in MOUNT_ATTR_STR_MAP  → mount_setattr(2) attrs
        //
        // "nosuid", "noexec", "nodev" are mount flags, NOT data options.
        // They belong in the flags field.
        spec.mounts = {
                // POSIX message queue filesystem — needed by some IPC libraries.
                OCIRuntime::Mount{
                        {},                                   // options (data)
                        {"nosuid", "noexec", "nodev"},        // flags → MS_NOSUID|MS_NOEXEC|MS_NODEV
                        {},                                   // attrs
                        "/dev/mqueue",                        // destination
                        "mqueue",                             // type
                        "mqueue"                              // source
                },
                // cgroup2 filesystem: mounted read-only so the container can
                // read its own resource limits but cannot modify host cgroups.
                OCIRuntime::Mount{
                        {},
                        {"nosuid", "noexec", "nodev", "relatime", "ro"},
                        {},
                        "/sys/fs/cgroup",
                        "cgroup2",
                        "cgroup2"
                }
        };

        // ── Devices ───────────────────────────────────────────────────────────
        // No extra device bind-mounts beyond what mount_necessary_dirs() provides
        // (null, zero, full, random, urandom via MS_BIND).
        spec.devices = {};

        // ── Network ───────────────────────────────────────────────────────────
        // No inbound port forwarding by default. Container has outbound
        // connectivity via pasta --config-net.
        // Populate tcp_ports / udp_ports for port forwarding, or set
        // auto_tcp / auto_udp to mirror host ports automatically.
        spec.networks = OCIRuntime::Network{
                {},     // tcp_ports
                {},     // udp_ports
                false,  // auto_tcp
                false   // auto_udp
        };

        // ── Masked paths ──────────────────────────────────────────────────────
        // These paths are obscured inside the container by bind-mounting /dev/null
        // over files or an empty tmpfs over directories. This prevents the
        // container from reading sensitive host kernel interfaces.
        spec.masked_paths = OCIRuntime::MaskedPaths{{
                "/proc/acpi",
                "/proc/kcore",
                "/proc/keys",
                "/proc/latency_stats",
                "/proc/sched_debug",
                "/proc/scsi",
                "/proc/timer_list",
                "/proc/timer_stats",
                "/sys/devices/virtual/powercap",
                "/sys/firmware",
                "/sys/fs/selinux",
                "/proc/interrupts"
                // Note: /sys/devices/virtual/powercap appears twice in the
                // reference config.json — the duplicate is intentionally omitted.
        }};

        // ── Read-only paths ───────────────────────────────────────────────────
        // These paths are remounted read-only inside the container. The container
        // can read these proc entries but cannot modify host kernel parameters.
        spec.read_only_paths = OCIRuntime::ReadOnlyPaths{{
                "/proc/asound",
                "/proc/bus",
                "/proc/fs",
                "/proc/irq",
                "/proc/sys",
                "/proc/sysrq-trigger"
        }};

        // ── Seccomp profile ───────────────────────────────────────────────────
        // Built by build_seccomp_profile(). Default action is SCMP_ACT_ERRNO(38)
        // (ENOSYS). SeccompProfileManager is only instantiated when
        // default_action is non-empty, so an empty Seccomp{} disables seccomp.
        spec.seccomp = build_seccomp_profile();

        // ── Time offsets ──────────────────────────────────────────────────────
        // Empty: no time namespace clock offsets applied.
        // Populate for containers that need a shifted monotonic or boottime clock.
        spec.timeoffsets = {};

        // ── Runtime-populated fields (for documentation) ──────────────────────
        // The following fields are intentionally left zero-initialized here.
        // They are populated by the runtime components listed:
        //
        //   pty_slave_fd   → PtySessionManager::setup_pty()
        //   pty_slave_name → PtySessionManager::setup_pty()
        //   control_sock   → ContainerMonitor::attach_to_stdio()
        //   pid            → ContainerMonitor after fork() of container child
        //   net_pid        → PastaNetwork::setup_networking()

        return spec;
}

} // namespace SpecGenerator
