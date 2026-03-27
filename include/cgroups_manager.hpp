#pragma once
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

class CGroupsManager {
        public:
                CGroupsManager() = default;
                ~CGroupsManager();
                CGroupsManager(const CGroupsManager&) = delete;
                CGroupsManager(CGroupsManager&&) = delete;
                auto operator=(const CGroupsManager&) -> CGroupsManager& = delete;
                auto operator=(CGroupsManager&&) -> CGroupsManager& = delete;

                auto init() -> void;
        private:
                fs::path m_cgroups_path{};
                std::ofstream m_file{};
};
