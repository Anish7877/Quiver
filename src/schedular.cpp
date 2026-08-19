#include "schedular.hpp"
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sched.h>
#include <unistd.h>
#include <iostream>
#include <format>
#include <stdexcept>
#include <unordered_map>

#ifndef SCHED_ISO
#define SCHED_ISO 4
#endif
#ifndef SCHED_IDLE
#define SCHED_IDLE 5
#endif
#ifndef SCHED_DEADLINE
#define SCHED_DEADLINE 6
#endif

#ifndef SCHED_FLAG_RESET_ON_FORK
#define SCHED_FLAG_RESET_ON_FORK 0x01
#define SCHED_FLAG_RECLAIM       0x02
#define SCHED_FLAG_DL_OVERRUN    0x04
#define SCHED_FLAG_KEEP_POLICY   0x08
#define SCHED_FLAG_KEEP_PARAMS   0x10
#define SCHED_FLAG_UTIL_CLAMP_MIN 0x20
#define SCHED_FLAG_UTIL_CLAMP_MAX 0x40
#endif


auto Schedular::apply_opts(const OCIRuntime::SchedularOpts& opts) -> void {
        if (opts.nice != 0) {
                if (setpriority(PRIO_PROCESS, 0, opts.nice) == -1) {
                        if (errno == EACCES || errno == EPERM) {
                                std::cerr << std::format("WARN: Rootless mode. Permission denied to set nice value to {}. Ignoring.\n", opts.nice);
                        } else {
                                throw std::runtime_error(std::format("Scheduler Error: Failed to set nice value. Errno: {}", errno));
                        }
                }
        }

        if (opts.policy.empty()) return;

        auto policy_it{OCIRuntime::SCHEDULAR_POLICY_STR_MAP.find(opts.policy)};
        if (policy_it == OCIRuntime::SCHEDULAR_POLICY_STR_MAP.end()) {
                throw std::invalid_argument(std::format("Scheduler Error: Unknown policy '{}'", opts.policy));
        }
        int policy{policy_it->second};

        if (policy == SCHED_DEADLINE || !opts.flags.empty()) {
                struct sched_attr attr{};
                attr.size = sizeof(attr);
                attr.sched_policy = policy;
                attr.sched_runtime = opts.runtime;
                attr.sched_deadline = opts.deadline;
                attr.sched_period = opts.period;
                attr.sched_priority = opts.priority;
                attr.sched_nice = opts.nice;

                for (const auto& flag_str : opts.flags) {
                        auto flag_it{OCIRuntime::SCHEDULAR_FLAGS_STR_MAP.find(flag_str)};
                        if (flag_it == OCIRuntime::SCHEDULAR_FLAGS_STR_MAP.end()) {
                                throw std::invalid_argument(std::format("Scheduler Error: Unknown flag '{}'", flag_str));
                        }
                        attr.sched_flags |= static_cast<uint64_t>(flag_it->second);
                }

                if (syscall(SYS_sched_setattr, 0, &attr, 0) == -1) {
                        if (errno == EPERM) {
                                std::cerr << std::format("WARN: Rootless mode. Permission denied to set SCHED_DEADLINE or advanced flags. Ignoring.\n");
                        } else {
                                throw std::runtime_error(std::format("Scheduler Error: Failed to set advanced attributes. Errno: {}", errno));
                        }
                }
        }
        else {
                struct sched_param param{};
                if (policy == SCHED_FIFO || policy == SCHED_RR) {
                        param.sched_priority = opts.priority;
                } else {
                        param.sched_priority = 0;
                }

                if (sched_setscheduler(0, policy, &param) == -1) {
                        if (errno == EPERM) {
                                std::cerr << std::format("WARN: Rootless mode. Permission denied to set real-time policy '{}'. Ignoring.\n", opts.policy);
                        } else {
                                throw std::runtime_error(std::format("Scheduler Error: Failed to set policy '{}'. Errno: {}", opts.policy, errno));
                        }
                }
        }
}
