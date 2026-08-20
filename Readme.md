<p align="center">
  <img src="logo.jpg" alt="Quiver logo" width="220">
</p>

<h1 align="center">Quiver</h1>

<p align="center">
  A rootless, OCI-compatible container runtime and CLI, written from scratch in modern C++.
</p>

<p align="center">
  <img alt="language" src="https://img.shields.io/badge/language-C%2B%2B20-blue">
  <img alt="platform" src="https://img.shields.io/badge/platform-Linux-lightgrey">
  <img alt="license" src="https://img.shields.io/badge/license-GPLv3-green">
</p>

---

## What is Quiver?

Quiver is a Docker/Podman-style container engine implemented directly on top of Linux primitives — namespaces, cgroups v2, seccomp, and capabilities — rather than wrapping an existing runtime. It runs **rootless by default**, manages its own image and container state, and can build container images from a `Quiverfile` (Dockerfile-compatible syntax) using an embedded, rootless BuildKit backend.

It's built as a single static-ish binary (`quiver`) with no daemon required for day-to-day container lifecycle operations.

## Key Features

- **Rootless containers** — runs unprivileged, using user namespaces, `newuidmap`/`newgidmap`, and delegated cgroups v2 controllers via systemd.
- **Full container lifecycle** — create, run, start, stop, pause/unpause, restart, kill, wait, remove.
- **Interactive & detached execution** — TTY allocation, `stdin`/`stdout` attach, `exec` into running containers.
- **Image building** — builds OCI images from a `Quiverfile` using a rootless [BuildKit](https://github.com/moby/buildkit) backend (Dockerfile-syntax compatible), with build args, targets, and layer caching.
- **Image management** — pull from a registry, load from a local OCI tarball, list, and remove images.
- **Resource control** — CPU quota/period/weight, CPU pinning (`cpuset`), memory limits and swap, PID limits, block-IO weight/limits, all via cgroups v2.
- **Filesystem & volumes** — bind mounts, `tmpfs`, read-only roots/paths, path masking, and configurable mount propagation.
- **Networking** — rootless container networking via [`pasta`](https://passt.top/), with TCP/UDP port publishing.
- **Security** — Linux capability add/drop, seccomp profiles, `no-new-privileges`, and other security options.
- **Live inspection** — `ps`, `inspect`, `stats`, `top` for container state, resource usage, and running processes.
- **systemd integration** — generate systemd unit files for containers (`generate-systemd`).
- **Fast, embedded state store** — container/image metadata is persisted with RocksDB, using FlatBuffers for serialization and BLAKE3 for content hashing.

## Architecture

| Concern | Technology |
|---|---|
| Container isolation | Linux namespaces (PID, mount, UTS, IPC, user, cgroup, time), cgroups v2 |
| Sandboxing | `libseccomp`, `libcap` |
| Networking | `pasta` (rootless user-mode networking) |
| Cgroup delegation | `systemd` (via `sdbus-c++`) for rootless cgroups v2 |
| State persistence | RocksDB (container/image databases) |
| Serialization | FlatBuffers (`flatbuffer_schemas/`) |
| Content hashing | BLAKE3 |
| Archive/layer handling | `libarchive` |
| Registry/HTTP | `cpr` (libcurl wrapper) |
| Image builds | Rootless BuildKit (`buildkitd` + `buildctl`, launched via `rootlesskit`) |

## Prerequisites

Quiver targets modern Linux only. You'll need:

- Linux with cgroups v2 and user namespaces enabled
- `g++` **13** or newer (C++20)
- [`flatc`](https://github.com/google/flatbuffers) (FlatBuffers compiler)
- [Conan](https://conan.io/) 2.x (C++ package manager, installed automatically by `setup.sh`)
- `cmake`, `autoconf`, `automake`, `libtool`, `pkg-config`, `python3`, `git`
- `newuidmap` / `newgidmap` (usually from a `uidmap`/`shadow-utils` package)
- `fuse-overlayfs`, `passt` (provides `pasta`, used for rootless networking)
- `rootlesskit`, `buildkitd`, `buildctl` (only required to use `quiver build`)
- `systemd` with a user session (for rootless cgroup delegation) and `sudo` access for one-time setup

## Installation

```bash
git clone https://github.com/Anish7877/Quiver.git
cd Quiver

# One-time host setup: installs build tools, fetches Conan dependencies,
# builds libseccomp/libattr/libacl/sdbus-c++/nlohmann-json from source if
# missing, and configures rootless cgroup v2 delegation via systemd.
./setup.sh

# Log out and back in (or restart your terminal) so the systemd/cgroup
# changes made by setup.sh take effect.

# Build the release binary
make
```

The compiled binary is placed at `./bin/release/quiver`. Add it to your `PATH`, e.g.:

```bash
sudo install -m 755 ./build/release/quiver /usr/local/bin/quiver
```

## Quick Start

```bash
# Pull an image from a registry
quiver image pull ubuntu:22.04

# Run a container interactively
quiver run -it ubuntu:22.04 /bin/bash

# Run detached, with a name, published port, and a bind mount
quiver run -d --name web \
  -p 8080:80 \
  -v ./site:/usr/share/nginx/html:ro \
  nginx:latest

# List running containers
quiver ps

# List all containers (including stopped)
quiver ps -a

# Inspect a container's full config
quiver inspect <container id>

# Follow live resource usage
quiver stats

# Stop and remove
quiver stop <container id>
quiver rm <container id>
```

## Building Images with a Quiverfile

Quiver builds images using its embedded rootless BuildKit backend. Write a `Quiverfile` using standard Dockerfile syntax:

```dockerfile
FROM alpine:3.20
RUN apk add --no-cache curl
COPY ./app /app
CMD ["/app/start.sh"]
```

Then build it:

```bash
quiver build -t myapp:latest .
```

Useful flags:

| Flag | Description |
|---|---|
| `-t, --tag <name>` | Tag the resulting image |
| `-f, --file <path>` | Use a Quiverfile at a custom path (default: `<context>/Quiverfile`) |
| `-o, --output <path>` | Output destination for the built image |
| `--build-arg <k=v>` | Pass a build argument |
| `--target <stage>` | Build a specific stage in a multi-stage build |
| `--no-cache` | Disable build cache |
| `--pull` | Always pull the base image |

## Command Reference

```
quiver run [OPTIONS] <image> [command] [arg...]   Create and start a new container
quiver create [OPTIONS] <image> [command] [arg...] Create a container without starting it
quiver start <container>...                        Start one or more stopped containers
quiver stop <container>...                          Stop one or more running containers
quiver restart <container>...                       Restart one or more containers
quiver pause <container>                            Pause a running container
quiver unpause <container>                           Resume a paused container
quiver kill <container>                             Send a signal to a container's main process
quiver wait <container>                              Block until a container exits
quiver rm <container>...                            Remove one or more containers
quiver ps [-a]                                       List containers (running, or all with -a)
quiver inspect <container>                           Show full container configuration
quiver stats                                         Live CPU/memory/IO usage for containers
quiver top <container>                               List processes running inside a container
quiver attach <container>                            Attach stdin/stdout/stderr to a running container
quiver exec <container> <command> [arg...]           Run a command inside a running container
quiver cp <src> <dest>                                Copy files to/from a container
quiver ports                                          List published ports per container
quiver mount ls|add|rm                               Manage bind mounts / volumes for a container
quiver update [OPTIONS] <container>                   Update resource limits on an existing container
quiver prune                                          Remove stopped containers / unused resources
quiver generate-systemd <container>                   Generate a systemd unit file for a container

quiver build [OPTIONS] <path>                        Build an image from a Quiverfile
quiver image ls                                       List local images
quiver image pull <image[:tag]>                       Pull an image from a registry
quiver image load <name[:tag]> <tar_path>              Load an image from an OCI tarball
quiver image rm <image[:tag]>...                       Remove one or more images

quiver help                                          Show CLI usage
```

Run `quiver help` at any time for the full, up-to-date list of flags (networking, security, resource limits, namespace joining, and scheduler options for `run`).

## Contributing

Issues and pull requests are welcome. Since Quiver talks directly to Linux kernel interfaces (namespaces, cgroups v2, seccomp), please test changes on a Linux machine with a systemd user session and cgroups v2 enabled.

## License

Quiver is licensed under the [GNU General Public License v3.0](./LICENSE).
