# Quiver Container Runtime

A lightweight container runtime proof of concept (PoC) that provides basic container management capabilities including creation, attachment, detachment, and lifecycle management.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Building the Project](#building-the-project)
- [Running a Container](#running-a-container)
- [Container Management](#container-management)
- [Usage Examples](#usage-examples)
- [Container Lifecycle](#container-lifecycle)

## Prerequisites

Before you can build and run Quiver, you need to install its dependencies.

### Arch Linux

On Arch Linux and its derivatives, you can install the required dependencies using `pacman`:

```bash
sudo pacman -S cpr nlohmann-json make
```

### Other Distributions

For other Linux distributions, ensure you have the following packages installed:
- **cpr** - C++ HTTP client library
- **nlohmann-json** - JSON library for C++
- **make** - Build automation tool

## Building the Project

1. Navigate to your Quiver working directory:
   ```bash
   cd ~/desktop/quiver
   ```

2. Build the project in debug mode:
   ```bash
   make build-debug
   ```

3. Navigate to the build directory:
   ```bash
   cd ./build/debug/
   ```

## Running a Container

To create and run a container, use the following command:

```bash
./quiver <image_name>:<version>
```

**Example:**
```bash
./quiver python:latest
```

After running this command, the output will display the container's attach socket path:
```
Container attach socket: /home/anish/.quiver/containers/<pid>/attach.sock
```

## Container Management

### Attaching to a Container

To attach to a running container, use the attach command with the socket path:

```bash
./quiver attach <path_to_container_socket>
```

**Example:**
```bash
./quiver attach /home/anish/.quiver/containers/<pid>/attach.sock
```

Once attached, you'll have an interactive session with the container.

### Detaching from a Container

To detach from a container without stopping it, use the key combination:

```
Ctrl+P, then Ctrl+Q
```

This will return you to the host shell while keeping the container running in the background.

### Re-attaching to a Container

After detaching, you can re-attach to the same container using the same attach command:

```bash
./quiver attach /home/anish/.quiver/containers/<pid>/attach.sock
```

### Stopping a Container

To stop and remove a container, simply type `exit` within the attached container session:

```bash
exit
```

The container will be automatically closed and removed from the system.

## Usage Examples

### Complete Workflow Example

1. **Start a Python container:**
   ```bash
   cd ~/desktop/quiver
   make build-debug
   cd ./build/debug/
   ./quiver python:latest
   ```

2. **Note the socket path from output:**
   ```
   Container attach socket: /home/anish/.quiver/containers/12345/attach.sock
   ```

3. **Attach to the container:**
   ```bash
   ./quiver attach /home/anish/.quiver/containers/12345/attach.sock
   ```

4. **Work inside the container:**
   ```bash
   python --version
   # Do your work...
   ```

5. **Detach (optional):**
   ```
   Ctrl+P, Ctrl+Q
   ```

6. **Re-attach (if detached):**
   ```bash
   ./quiver attach /home/anish/.quiver/containers/12345/attach.sock
   ```

7. **Stop the container:**
   ```bash
   exit
   ```

## Container Lifecycle

The Quiver container runtime follows this lifecycle:

1. **Creation**: Container is created and started with `./quiver <image>:<tag>`
2. **Running**: Container runs in the background with an attach socket
3. **Attachment**: Interactive session via `./quiver attach <socket_path>`
4. **Detachment**: Session detached with `Ctrl+P, Ctrl+Q` (container continues running)
5. **Termination**: Container stopped and removed with `exit` command

## Notes

- Container sockets are stored in `~/.quiver/containers/<pid>/` directory
- Each container has a unique process ID (PID) for identification
- Containers are automatically cleaned up when terminated with `exit`
- Multiple attach/detach cycles are supported for the same container

## Troubleshooting

- Ensure all dependencies are installed before building
- Make sure you have proper permissions to create directories in your home folder
- Check that the container image exists and is accessible
- Verify the socket path exists before attempting to attach
