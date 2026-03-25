#include "mount.hpp"
#include <sys/mount.h>

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

auto Mount::_tmpfs(const fs::path& fs, const std::string& opts) -> bool {
        if (mount("tmpfs", fs.c_str(), "tmpfs", MS_NODEV | MS_NOSUID, opts.c_str()) == -1) [[unlikely]] {
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

auto Mount::_volumes(const std::vector<std::pair<fs::path, fs::path>>& paths) -> bool {
        for (const auto& path : paths) {
                int flags{MS_BIND | MS_REC | MS_NODEV | MS_NOSUID};
                if (mount(path.first.c_str(), path.second.c_str(), NULL, flags, NULL) == -1) [[unlikely]] {
                        return false;
                }
        }
        return true;
}

auto Mount::_devices(const std::vector<std::pair<fs::path, fs::path>>& paths) -> bool {
        for (const auto& path : paths) {
                int flags{MS_BIND | MS_REC | MS_NOSUID | MS_NOEXEC};
                if (mount(path.first.c_str(), path.second.c_str(), NULL, flags, NULL) == -1) [[unlikely]] {
                        return false;
                }
        }
        return true;
}
