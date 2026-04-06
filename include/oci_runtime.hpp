#pragma once
#include <cstdint>
#include <sched.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <unordered_map>
#include <sys/resource.h>
#include <linux/ioprio.h>
#include <sys/mount.h>
#include <linux/mount.h>
#include <seccomp.h>
#include <filesystem>
namespace fs = std::filesystem;

namespace OCIRuntime {

        const std::unordered_map<std::string, int> SCHEDULAR_POLICY_STR_MAP {
                {"other", SCHED_OTHER},
                {"fifo", SCHED_FIFO},
                {"rr", SCHED_RR},
                {"batch", SCHED_BATCH},
                {"iso", SCHED_ISO},
                {"idle", SCHED_IDLE},
                {"deadline", SCHED_DEADLINE}
        };

        const std::unordered_map<std::string, int> SCHEDULAR_FLAGS_STR_MAP {
                {"reset_on_fork", SCHED_FLAG_RESET_ON_FORK},
                {"reclaim", SCHED_FLAG_RECLAIM},
                {"dl_overrun", SCHED_FLAG_DL_OVERRUN},
                {"keep_policy", SCHED_FLAG_KEEP_POLICY},
                {"keep_params", SCHED_FLAG_KEEP_PARAMS},
                {"util_clamp_min", SCHED_FLAG_UTIL_CLAMP_MIN},
                {"util_clamp_max", SCHED_FLAG_UTIL_CLAMP_MAX}
        };

        const std::unordered_map<std::string, int> IOPRIO_CLASS_STR_MAP {
                {"rt", IOPRIO_CLASS_RT},
                {"be", IOPRIO_CLASS_BE},
                {"idle", IOPRIO_CLASS_IDLE}
        };

        const std::unordered_map<std::string, int> RLIMIT_STR_MAP {
                {"cpu", RLIMIT_CPU},
                {"fsize", RLIMIT_FSIZE},
                {"data", RLIMIT_DATA},
                {"stack", RLIMIT_STACK},
                {"core", RLIMIT_CORE},
                {"rss", RLIMIT_RSS},
                {"nproc", RLIMIT_NPROC},
                {"nofile", RLIMIT_NOFILE},
                {"memlock", RLIMIT_MEMLOCK},
                {"as", RLIMIT_AS},
                {"locks", RLIMIT_LOCKS},
                {"sigpending", RLIMIT_SIGPENDING},
                {"msgqueue", RLIMIT_MSGQUEUE},
                {"nice", RLIMIT_NICE},
                {"rtprio", RLIMIT_RTPRIO},
                {"rttime", RLIMIT_RTTIME}
        };

        const std::unordered_map<std::string, int> ROOTFS_PROPAGATION_STR_MAP {
                {"slave", MS_SLAVE},
                {"private", MS_PRIVATE},
                {"shared", MS_SHARED},
                {"unbindable", MS_UNBINDABLE}
        };

        const std::unordered_map<std::string, int> SCMP_SYSCALL_OP_STR_MAP {
                {"ne", SCMP_CMP_NE},
                {"lt", SCMP_CMP_LT},
                {"le", SCMP_CMP_LE},
                {"eq", SCMP_CMP_EQ},
                {"ge", SCMP_CMP_GE},
                {"gt", SCMP_CMP_GT},
                {"masked_eq", SCMP_CMP_MASKED_EQ}
        };

        const std::unordered_map<std::string, int> SCMP_ARCH_STR_MAP { {"x86", SCMP_ARCH_X86},
                {"x86_64", SCMP_ARCH_X86_64},
                {"x32", SCMP_ARCH_X32}
        };

        const std::unordered_map<std::string, int> SCMP_FLAGS_STR_MAP {
                {"tsync", SECCOMP_FILTER_FLAG_TSYNC},
                {"log", SECCOMP_FILTER_FLAG_LOG},
                {"spec_allow", SECCOMP_FILTER_FLAG_SPEC_ALLOW},
                {"wait_killable_recv", SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV}
        };

        const std::unordered_map<std::string, int> TIMEOFFSET_STR_MAP {
                {"monotonic", CLOCK_MONOTONIC},
                {"boottime", CLOCK_BOOTTIME}
        };

        const std::unordered_map<std::string, int> NAMESPACE_STR_MAP {
                {"pid", CLONE_NEWPID},
                {"network", CLONE_NEWNET},
                {"ipc", CLONE_NEWIPC},
                {"uts", CLONE_NEWUTS},
                {"mount", CLONE_NEWNS},
                {"cgroup", CLONE_NEWCGROUP},
                {"time", CLONE_NEWTIME}
        };

        const std::unordered_map<std::string, unsigned long> MOUNT_FLAGS_STR_MAP {
                {"async", 0},
                {"atime", 0},
                {"bind", MS_BIND},
                {"defaults", 0},
                {"dev", 0},
                {"diratime", 0},
                {"dirsync", MS_DIRSYNC},
                {"exec", 0},
                {"iversion", MS_I_VERSION},
                {"lazytime", MS_LAZYTIME},
                {"loud", 0},
                {"mand", MS_MANDLOCK},
                {"noatime", MS_NOATIME},
                {"nodev", MS_NODEV},
                {"nodiratime", MS_NODIRATIME},
                {"noexec", MS_NOEXEC},
                {"noiversion", 0},
                {"nolazytime", 0},
                {"nomand", 0},
                {"norelatime", 0},
                {"nostrictatime", 0},
                {"nosuid", MS_NOSUID},
                {"nosymfollow", MS_NOSYMFOLLOW},
                {"private", MS_PRIVATE},
                {"rbind", MS_BIND | MS_REC},
                {"relatime", MS_RELATIME},
                {"remount", MS_REMOUNT},
                {"ro", MS_RDONLY},
                {"rprivate", MS_PRIVATE | MS_REC},
                {"rshared", MS_SHARED | MS_REC},
                {"rslave", MS_SLAVE | MS_REC},
                {"runbindable", MS_UNBINDABLE | MS_REC},
                {"rw", 0},
                {"shared", MS_SHARED},
                {"silent", MS_SILENT},
                {"slave", MS_SLAVE},
                {"strictatime", MS_STRICTATIME},
                {"suid", 0},
                {"symfollow", 0},
                {"sync", MS_SYNCHRONOUS},
                {"unbindable", MS_UNBINDABLE}
        };

        const std::unordered_map<std::string, unsigned long> MOUNT_ATTR_STR_MAP {
                {"rnoatime", MOUNT_ATTR_NOATIME},
                {"rnodiratime", MOUNT_ATTR_NODIRATIME},
                {"rnoexec", MOUNT_ATTR_NOEXEC},
                {"rnosuid", MOUNT_ATTR_NOSUID},
                {"rro", MOUNT_ATTR_RDONLY},
                {"rstrictatime", MOUNT_ATTR_STRICTATIME},
                {"rnosymfollow", MOUNT_ATTR_NOSYMFOLLOW},
                {"idmap", MOUNT_ATTR_IDMAP},
                {"ridmap", MOUNT_ATTR_IDMAP}
        };

        struct Root {
                fs::path path{};
                bool read_only{};
        };

        struct Terminal {
                bool value{};
        };

        struct ConsoleSize {
                std::uint32_t height{};
                std::uint32_t width{};
        };

        struct User {
                uid_t uid{};
                gid_t gid{};
                std::uint32_t umask{};
                std::vector<gid_t> additional_gids{};
        };

        struct UidMapping {
                std::uint32_t container_id{};
                std::uint32_t host_id{};
                std::uint32_t size{};
        };

        struct GidMapping {
                std::uint32_t container_id{};
                std::uint32_t host_id{};
                std::uint32_t size{};
        };

        struct Env {
                std::vector<std::string> value{};
        };

        struct Cwd {
                std::string value{};
        };

        struct Args {
                std::vector<const char*> value{};
        };

        struct OomScoreAdj {
                std::int32_t value{};
        };

        struct SchedularOpts {
                std::vector<std::string> flags{};
                std::string policy{};
                std::uint64_t runtime{};
                std::uint64_t deadline{};
                std::uint64_t period{};
                std::int32_t nice{};
                std::int32_t priority{};
        };

        struct NoNewPrivileges {
                bool value{};
        };

        struct Capabilities {
                std::vector<std::string> bounding{};
                std::vector<std::string> effective{};
                std::vector<std::string> inheritable{};
                std::vector<std::string> permitted{};
                std::vector<std::string> ambient{};
        };

        struct RLimit {
                std::string name{};
                std::uint64_t hard_limit{};
                std::uint64_t soft_limit{};
        };

        struct RootfsPropagation {
                std::string type{};
        };

        struct Seccomp {
                struct Arg {
                        std::string op{};
                        std::uint64_t value{};
                        std::uint64_t value_two{};
                        std::uint32_t index{};
                };

                struct SyscallRule {
                        std::vector<Arg> args{};
                        std::vector<std::string> names{};
                        std::string action{};
                        std::uint32_t errno_ret{};
                };

                std::vector<SyscallRule> syscalls{};
                std::vector<std::string> archs{};
                std::vector<std::string> flags{};
                std::string default_action{};
                std::uint32_t default_errno{};
        };

        struct TimeOffset {
                std::string type{};
                std::int64_t secs{};
                std::int64_t nanosecs{};
        };

        struct Device {
                std::string type{};
                fs::path path{};
                std::int64_t major{};
                std::int64_t minor{};
                std::uint32_t fileMode{};
                uid_t uid{};
                gid_t gid{};
        };

        struct Namespace {
                fs::path path{};
                std::string type{};
        };

        struct Mount {
                std::vector<std::string> options{};
                std::vector<std::string> flags{};
                std::vector<std::string> attrs{};
                std::string destination{};
                std::string type{};
                std::string source{};
        };

        struct MaskedPaths {
                std::vector<fs::path> paths{};
        };

        struct ReadOnlyPaths {
                std::vector<fs::path> paths{};
        };
}
