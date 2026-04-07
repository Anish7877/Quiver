#include "mount.hpp"
#include "oci_runtime.hpp"
#include "utils.hpp"
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <format>
#include <cstring>
#include <sys/mount.h>
#include <unistd.h>

auto Mount::_set_propagation(const fs::path& fs, int flags) -> bool {
        if (mount("none", fs.c_str(), NULL, flags | MS_REC, NULL) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_overlay_fs(const fs::path& fs, const std::string& overlay_opts) -> bool {
        if (mount("overlay", fs.c_str(), "overlay", MS_NODEV, overlay_opts.c_str()) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_new_filesystem(const fs::path& fs) -> bool {
        if (mount(fs.c_str(), fs.c_str(), NULL, MS_BIND | MS_REC, NULL) == -1) [[unlikely]] {
                return false;
        }
        if (mount("none", fs.c_str(), NULL, MS_PRIVATE | MS_REC, NULL) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_private(const fs::path& fs) -> bool {
        if (mount("none", fs.c_str(), NULL, MS_PRIVATE | MS_REC, NULL) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_proc() -> bool {
        if (mount("proc","/proc", "proc", MS_NOEXEC | MS_NODEV | MS_NOSUID, NULL) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_sys() -> bool {
        if (mount("sysfs","/sys", "sysfs", MS_RDONLY | MS_NOEXEC | MS_NODEV | MS_NOSUID, NULL) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_dev() -> bool {
        if (mount("tmpfs","/dev", "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755,size=65536K") == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_tmpfs(const fs::path& fs, const std::string& opts) -> bool {
        if (mount("tmpfs", fs.c_str(), "tmpfs", MS_NODEV | MS_NOSUID, opts.c_str()) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_devpts(const fs::path& fs, const std::string& opts) -> bool {
        if (mount("devpts", fs.c_str(), "devpts", MS_NOEXEC | MS_NOSUID, opts.c_str()) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_unmount_filesystem(const fs::path& fs) -> bool {
        if (umount2(fs.c_str(), MNT_DETACH) == -1) [[unlikely]] {
                return false;
        }
        return true;
}

auto Mount::_volumes(const std::vector<OCIRuntime::Mount>& mounts, const fs::path& container_root) -> void {
        for (const auto& mnt : mounts) {
                std::string rel_path{mnt.destination};
                if (!rel_path.empty() && rel_path[0] == '/') {
                        rel_path = rel_path.substr(1);
                }
                fs::path actual_target{container_root / rel_path};

                try {
                        if (!fs::exists(mnt.source)) {
                                std::cerr << std::format("Warn: Source path does not exist on host -> '{}'. Skipping.\n", mnt.source);
                                continue;
                        }
                        if (fs::is_directory(mnt.source)) {
                                Utils::ensure_dir(actual_target);
                        }
                        else {
                                Utils::ensure_file(actual_target);
                        }
                }
                catch (const std::exception& e) {
                        std::cerr << std::format("Warn: Target creation failed for volume '{}' -> {}\n", mnt.destination, e.what());
                        continue;
                }

                int default_flags{MS_BIND | MS_REC | MS_NODEV | MS_NOSUID};
                int flags{MS_BIND | MS_REC};
                int mnt_attr_flags{0};
                std::string mnt_opts{""};

                for (const auto& flag : mnt.flags) {
                        auto it{OCIRuntime::MOUNT_FLAGS_STR_MAP.find(flag)};
                        if (it != OCIRuntime::MOUNT_FLAGS_STR_MAP.end()) {
                                flags |= it->second;
                        }
                        else [[unlikely]] {
                                std::cerr << std::format("Warn: Ignoring unknown mnt flag -> '{}'.\n", flag);
                        }
                }

                for (const auto& mnt_attr : mnt.attrs) {
                        auto it{OCIRuntime::MOUNT_ATTR_STR_MAP.find(mnt_attr)};
                        if (it != OCIRuntime::MOUNT_ATTR_STR_MAP.end()) {
                                mnt_attr_flags |= it->second;
                        }
                        else [[unlikely]] {
                                std::cerr << std::format("Warn: Ignoring unknown mnt attribute -> '{}'.\n", mnt_attr);
                        }
                }

                for (const auto& opt : mnt.options) {
                        mnt_opts += opt;
                        mnt_opts += ',';
                }

                if (mnt.flags.empty()) flags = default_flags;

                if (!mnt_opts.empty()) {
                        mnt_opts.pop_back();
                }
                const char* data_ptr{mnt_opts.empty() ? nullptr : mnt_opts.c_str()};
                const char* type_ptr{mnt.type.empty() ? nullptr : mnt.type.c_str()};
                if (mount(mnt.source.c_str(), actual_target.c_str(), type_ptr, flags, data_ptr) == -1) [[unlikely]] {
                        std::cerr << std::format("Warn: Unable to mount volume -> '{}' to '{}' : {}.\n",
                                        mnt.source, actual_target.string(), std::strerror(errno));
                        continue;
                }
                if ((flags & MS_BIND) && (flags & MS_RDONLY)) {
                        unsigned int remount_flags{flags | MS_REMOUNT};
                        if (mount(mnt.source.c_str(), actual_target.c_str(), type_ptr, remount_flags, nullptr) == -1) {
                                std::cerr << std::format("Warn: Unable to remount volume as read-only -> '{}' : {}.\n",
                                                actual_target.string(), std::strerror(errno));
                        }
                }
        }
}

auto Mount::_devices(const std::vector<OCIRuntime::Device>& devices, const fs::path& container_root) -> void {
        for (const auto& device : devices) {
                if (!fs::exists(device.host_path)) {
                        std::cerr << std::format("Warn: Device path does not exist on host -> '{}'. Skipping.\n", device.host_path.string());
                        continue;
                }
                std::string rel_path{device.container_path.string()};
                if (!rel_path.empty() && rel_path[0] == '/') {
                        rel_path = rel_path.substr(1);
                }
                fs::path actual_target{container_root / rel_path};
                try {
                        Utils::ensure_dir(actual_target.parent_path());
                }
                catch (const std::exception& e) {
                        std::cerr << e.what() << '\n';
                        continue;
                }
                int fd{open(actual_target.c_str(), O_CREAT | O_RDONLY, 0644)};
                if (fd == -1) [[unlikely]] {
                        std::cerr << std::format("Warn: Cannot create target device file -> '{}' : {}.\n",
                                                 actual_target.string(), std::strerror(errno));
                        continue;
                }
                close(fd);

                int flags{MS_BIND | MS_REC | MS_NOSUID | MS_NOEXEC};
                if (mount(device.host_path.c_str(), actual_target.c_str(), NULL, flags, NULL) == -1) [[unlikely]] {
                        std::cerr << std::format("Warn: Unable to mount device -> '{}' to '{}' : {}.\n",
                                                 device.host_path.string(), actual_target.string(), std::strerror(errno));
                        continue;
                }
        }
}
