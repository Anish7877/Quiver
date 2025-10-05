#pragma once
#define PROC "proc"
#define DEV "dev"
#define SYS "sys"
#define TMPFS "tmpfs"
#define DEVPTS "devps"
#include <string>
#include <vector>

namespace Mount{
    void proc(const std::string& proc_path, const int& flags);
    void dev(const std::string& dev_path, const int& flags);
    void sys(const std::string& sys_path, const int& flags);
    void tmpfs(const std::string& tmpfs_path, const int& flags);
    void dev_pts(const std::string& dev_pts_path, const int& flags);
    void volumes(const std::string& rootfs, const std::vector<std::string> volumes);
}
