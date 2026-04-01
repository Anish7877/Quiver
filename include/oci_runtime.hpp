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
#include <seccomp.h>
#include <filesystem>
namespace fs = std::filesystem;

namespace OCIRuntime {
        enum class CapabilityType : std::uint8_t {
                PERMITTED   = 0,
                EFFECTIVE   = 1,
                INHERITABLE = 2,
                BOUNDING    = 3,
                AMBIENT     = 4
        };

        const std::unordered_map<std::string, int> SCHEDULAR_POLICY_STR_MAP {
                {"OTHER", SCHED_OTHER},
                {"FIFO", SCHED_FIFO},
                {"RR", SCHED_RR},
                {"BATCH", SCHED_BATCH},
                {"ISO", SCHED_ISO},
                {"IDLE", SCHED_IDLE},
                {"DEADLINE", SCHED_DEADLINE}
        };

        const std::unordered_map<std::string, int> SCHEDULAR_FLAGS_STR_MAP {
                {"RESET_ON_FORK", SCHED_FLAG_RESET_ON_FORK},
                {"RECLAIM", SCHED_FLAG_RECLAIM},
                {"DL_OVERRUN", SCHED_FLAG_DL_OVERRUN},
                {"KEEP_POLICY", SCHED_FLAG_KEEP_POLICY},
                {"KEEP_PARAMS", SCHED_FLAG_KEEP_PARAMS},
                {"UTIL_CLAMP_MIN", SCHED_FLAG_UTIL_CLAMP_MIN},
                {"UTIL_CLAMP_MAX", SCHED_FLAG_UTIL_CLAMP_MAX}
        };

        const std::unordered_map<std::string, int> IOPRIO_CLASS_STR_MAP {
                {"RT", IOPRIO_CLASS_RT},
                {"BE", IOPRIO_CLASS_BE},
                {"IDLE", IOPRIO_CLASS_IDLE}
        };

        const std::unordered_map<std::string, int> RLIMIT_STR_MAP {
                {"CPU", RLIMIT_CPU},
                {"FSIZE", RLIMIT_FSIZE},
                {"DATA", RLIMIT_DATA},
                {"STACK", RLIMIT_STACK},
                {"CORE", RLIMIT_CORE},
                {"RSS", RLIMIT_RSS},
                {"NPROC", RLIMIT_NPROC},
                {"NOFILE", RLIMIT_NOFILE},
                {"MEMLOCK", RLIMIT_MEMLOCK},
                {"AS", RLIMIT_AS},
                {"LOCKS", RLIMIT_LOCKS},
                {"SIGPENDING", RLIMIT_SIGPENDING},
                {"MSGQUEUE", RLIMIT_MSGQUEUE},
                {"NICE", RLIMIT_NICE},
                {"RTPRIO", RLIMIT_RTPRIO},
                {"RTTIME", RLIMIT_RTTIME}
        };

        const std::unordered_map<std::string, int> ROOTFS_PROPAGATION_STR_MAP {
                {"SLAVE", MS_SLAVE},
                {"PRIVATE", MS_PRIVATE},
                {"SHARED", MS_SHARED},
                {"UNBINDABLE", MS_UNBINDABLE}
        };

        const std::unordered_map<std::string, int> SCMP_SYSCALL_OP_STR_MAP {
                {"NE", SCMP_CMP_NE},
                {"LT", SCMP_CMP_LT},
                {"LE", SCMP_CMP_LE},
                {"EQ", SCMP_CMP_EQ},
                {"GE", SCMP_CMP_GE},
                {"GT", SCMP_CMP_GT},
                {"MASKED_EQ", SCMP_CMP_MASKED_EQ}
        };

        const std::unordered_map<std::string, int> SCMP_ARCH_STR_MAP { {"X86", SCMP_ARCH_X86},
                {"X86_64", SCMP_ARCH_X86_64},
                {"X32", SCMP_ARCH_X32}
        };

        const std::unordered_map<std::string, int> SCMP_FLAGS_STR_MAP {
                {"TSYNC", SECCOMP_FILTER_FLAG_TSYNC},
                {"LOG", SECCOMP_FILTER_FLAG_LOG},
                {"SPEC_ALLOW", SECCOMP_FILTER_FLAG_SPEC_ALLOW},
                {"WAIT_KILLABLE_RECV", SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV}
        };

        const std::unordered_map<std::string, int> TIMEOFFSET_STR_MAP {
                {"MONOTONIC", CLOCK_MONOTONIC},
                {"BOOTTIME", CLOCK_BOOTTIME}
        };

        const std::unordered_map<std::string, int> NAMESPACE_STR_MAP {
                {"PID", CLONE_NEWPID},
                {"NETWORK", CLONE_NEWNET},
                {"IPC", CLONE_NEWIPC},
                {"UTS", CLONE_NEWUTS},
                {"MOUNT", CLONE_NEWNS},
                {"CGROUP", CLONE_NEWCGROUP},
                {"TIME", CLONE_NEWTIME}
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

        struct Env {
                std::vector<const char*> value{};
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

        struct IOPriority {
                std::string name{};
                std::uint8_t priority{};
        };

        struct NoNewPrivileges {
                bool value{};
        };

        struct Capability {
                std::string name{};
                CapabilityType type{};
        };

        struct RLimit {
                std::string name{};
                std::uint64_t hard_limit{};
                std::uint64_t soft_limit{};
        };

        struct CPUAffnity {
                std::string initial{};
                std::string final{};
        };

        struct Hook {
                std::vector<std::string> args{};
                std::vector<std::string> envs{};
                fs::path path{};
                std::string type{};
                std::uint32_t timeout{};
        };

        struct RootfsPropagation {
                std::string type{};
        };

        struct Seccomp {
                private:
                        struct arg {
                                std::string op{};
                                std::uint64_t value{};
                                std::uint64_t value_two{};
                                std::uint32_t index{};
                        };
                public:
                        struct syscalls {
                                std::vector<arg> args{};
                                std::vector<std::string> names{};
                                std::string action{};
                                std::uint32_t errno_ret{};
                        };
                        std::vector<std::string> archs{};
                        std::vector<std::string> flags{};
                        std::string default_action{};
                        std::uint32_t default_errno{};
        };

        struct TimeOffset {
                std::string type{};
                std::uint32_t secs{};
                std::uint32_t nanosecs{};
        };

        struct Namespace {
                std::string name{};
        };

        struct MaskedPath {
                fs::path path{};
        };

        struct ReadOnlyPath {
                fs::path path{};
        };

        struct Annotation {
                std::string key{};
                std::string value{};
        };
}
