#pragma once
#include "cgroups_manager_interface.hpp"
#include <memory>
#include <filesystem>
namespace fs = std::filesystem;

class CGroupsManagerCreator {
        public:
                static auto create_cgourps_manager(const std::string&, const fs::path&) -> std::unique_ptr<CGroupsManagerInterface>;
};
