#pragma once
#include <filesystem>
#include <vector>
namespace fs = std::filesystem;

namespace Mount {
        auto _overlay_fs(const fs::path&, const std::string&) -> bool;
        auto _new_filesystem(const fs::path&) -> bool;
        auto _private(const fs::path&) -> bool;
        auto _proc() -> bool;
        auto _sys() -> bool;
        auto _dev() -> bool;
        auto _tmpfs(const fs::path&, const std::string&) -> bool;
        auto _devpts(const fs::path&, const std::string&) -> bool;
        auto _unmount_filesystem(const fs::path&) -> bool;
        auto _volumes(const std::vector<std::pair<fs::path, fs::path>>&) -> bool;
        auto _devices(const std::vector<std::pair<fs::path, fs::path>>&) -> bool;
}
