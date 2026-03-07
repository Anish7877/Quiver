#include "container_runtime.hpp"

auto ContainerRuntime::exec_commands() -> void {
        //TODO: get the commands from parsed manifest and run it inside the container namespace
}

auto ContainerRuntime::pause_container() -> void {
        //TODO: store a checkpoint from where we can resume the container and stop the container
}

auto ContainerRuntime::unpause_container() -> void {
        //TODO: load the checkpoint stored in the pause and restart the container from that point
}

auto ContainerRuntime::restart_container() -> void {
        //TODO: restart container with the default config
}

auto ContainerRuntime::run_container() -> void {
        //TODO: get container config and start a namespace with that process
}
