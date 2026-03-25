#pragma once
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

class CGroupsManager {
        public:
                CGroupsManager() = default;
                auto init() -> void;
        private:
                fs::path m_cgroups_path{};
                std::ofstream m_file{};
};
