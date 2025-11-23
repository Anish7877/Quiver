<p align="left">
  <img src="./logo.png" alt="Quiver Logo" width="400" height="400" align="left" style="margin-right: 20px;">
</p>
<div style="padding-top: 60px;">
  <h1 style="font-size: 48px; margin: 0;">Quiver</h1>
  <p style="font-size: 20px; margin: 5px 0 0 0;">A Lightweight, Rootless Container Runtime for Linux</p>
</div>
<br clear="left">

## Overview

Quiver is a C++ based container runtime designed to manage and execute rootless containers using Linux namespaces and cgroups. It provides a command-line interface similar to Docker, allowing users to pull images, manage container life-cycles, handle networking via slirp4netns, and manage volumes.

Quiver uses SQLite to track container states and supports both OverlayFS and Virtual Filesystems (VFS) for storage management.

## Features

- **Rootless Execution**: Run containers without root privileges using user namespaces.
- **Image Management**: Pull images directly from Docker Hub.
- **Container Lifecycle**: Create, Start, Stop, Remove, and Attach to containers.
- **Networking**: User-mode networking with port forwarding support.
- **Storage**: Support for persistent volumes and OverlayFS.
- **Process Isolation**: PID, Mount, UTS, IPC, and Network namespace isolation.

## Prerequisites

To build and run Quiver, you need a Linux environment with the following dependencies installed:

### Build Dependencies

- **C++ Compiler**: GCC (g++) supporting C++17.
- **Libraries**:
  - libcpr (C++ Requests)
  - libcurl
  - openssl (libssl, libcrypto)
  - sqlite3 (libsqlite3)
  - libutil

### Runtime Dependencies

- **slirp4netns**: Required for rootless networking.
- **uidmap**: For handling user namespace mappings (newuidmap, newgidmap).

### Install Dependencies (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install build-essential g++ libsqlite3-dev libssl-dev libcurl4-openssl-dev libcpr-dev slirp4netns uidmap
```

## Installation & Build

1. **Clone the repository**:

```bash
git clone https://github.com/anish7877/quiver.git
cd quiver
```

2. **Build the project**:

You can build the release version using the provided Makefile.

```bash
make build-release
```

Alternatively, for debugging:

```bash
make build-debug
```

3. **Run Quiver**:

The binary is output to `./build/release/quiver`.

```bash
./build/release/quiver help
```

## Usage

### Basic Commands

**Pull an image**:

```bash
./build/release/quiver pull ubuntu:latest
```

**Run a container**:

```bash
# Run interactive bash shell
./build/release/quiver run -i ubuntu:latest -n my_container /bin/bash
```

**List containers**:

```bash
./build/release/quiver ps      # List running
./build/release/quiver ps -a   # List all
```

**Network Port Forwarding**:

```bash
# Map host port 8080 to container port 80
./build/release/quiver run -i nginx:latest -p 8080:80
```

**Bind Mount Volumes**:

```bash
./build/release/quiver run -i ubuntu:latest -v /home/user/data:/data
```

### Command Reference

| Command | Description |
|---------|-------------|
| `run` | Create and start a new container. Options: `-i` (image), `-n` (name), `-p` (port), `-v` (volume). |
| `start` | Start a stopped container by ID. |
| `stop` | Stop a running container by ID. |
| `rm` | Remove a container (must be stopped). |
| `attach` | Attach to a running container's terminal. |
| `image ls` | List downloaded images. |
| `image rm` | Remove a downloaded image. |
| `pull` | Download an image from a registry. |

## Troubleshooting

If you encounter issues while running Quiver, please refer to the known bugs and fixes below.

### 1. Filesystem Overwriting / Permission Errors

**Symptom**: You encounter errors related to OverlayFS permissions or filesystem overwriting.

**Fix**: Use the VFS (Virtual Filesystem) option. Note that this copies the filesystem, which takes more time and disk space.

```bash
./build/release/quiver run --vfs --no-remove -i <image_name>
```

- `--vfs`: Uses a copy-based filesystem instead of OverlayFS.
- `--no-remove`: Prevents deletion of data upon exit (useful for debugging).

### 2. Unprivileged Kernel Clone Errors

**Symptom**: The container fails to start with errors related to user namespaces or cloning.

**Fix**: Your kernel settings may be restricting unprivileged user namespaces.

**Temporary Fix**:

```bash
sudo sysctl -w kernel.unprivileged_userns_clone=1
```

**Permanent Fix**:

```bash
echo 'kernel.unprivileged_userns_clone=1' | sudo tee /etc/sysctl.d/00-local-userns.conf
sudo service procps restart
```

### 3. AppArmor Errors

**Symptom**: Permission denied errors specifically on Ubuntu or Debian-based distributions, referencing AppArmor.

**Fix**: AppArmor profiles may interfere with the container process.

```bash
sudo systemctl stop apparmor
sudo systemctl disable apparmor
```

## License

This project is licensed under the GNU General Public License v3.0. See the LICENSE file for details.
