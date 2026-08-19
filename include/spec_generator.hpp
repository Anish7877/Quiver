#pragma once
#include "container_config.hpp"
#include "oci_runtime.hpp"
#include <string>
#include <unistd.h>

namespace SpecGenerator {

        // Generates a default OCI-compliant rootless container spec.
        // Modelled after the Podman/crun default spec (OCI Runtime Spec 1.2.1).
        //
        // The caller is responsible for populating fields that are runtime-specific
        // and cannot be determined at spec-generation time:
        //   - pty_slave_fd  : set by PtySessionManager after openpty()
        //   - pty_slave_name: set by PtySessionManager after openpty()
        //   - control_sock  : set by ContainerMonitor::attach_to_stdio()
        //   - pid           : set by ContainerMonitor after fork()
        //   - net_pid       : set by PastaNetwork::setup_networking()
        //   - cgroups_path  : must be set by the caller on non-systemd hosts;
        //                     left empty so CGroupsManagerCreator selects
        //                     SystemdCGroupsManager when systemd is present
        //
        // Throws std::runtime_error on invalid input.
        [[nodiscard]] auto generate_default_rootless_spec(
                const std::string& container_id,
                const std::string& rootfs_path) -> ContainerConfig;

} // namespace SpecGenerator
