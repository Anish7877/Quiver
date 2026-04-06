#include "cgroups_manager_creator.hpp"
#include "systemd_cgroups_manager.hpp"
#include "raw_cgroups_manager.hpp"

auto CGroupsManagerCreator::create_cgourps_manager(const std::string& container_id, const fs::path& delegated_path) -> std::unique_ptr<CGroupsManagerInterface> {
        if (delegated_path.empty() && fs::exists("/run/systemd/system")) {
                return std::make_unique<SystemdCGroupsManager>(container_id);
        }
        return std::make_unique<RawCGroupsManager>(container_id, delegated_path);
}
