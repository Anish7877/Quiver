#include "systemd_cgroups_manager.hpp"
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <sdbus-c++/sdbus-c++.h>
using SystemdProperty = sdbus::Struct<std::string, sdbus::Variant>;

auto SystemdCGroupsManager::attach_process(pid_t pid) -> void {
        const std::string unit_name{std::format("quiver-{}.scope", m_container_id)};

        try {
                auto connection{sdbus::createSessionBusConnection()};
                auto systemd_proxy{sdbus::createProxy(*connection, sdbus::ServiceName{"org.freedesktop.systemd1"}, sdbus::ObjectPath{"/org/freedesktop/systemd1"})};

                std::vector<SystemdProperty> properties{};

                properties.emplace_back(sdbus::make_struct("PIDs", sdbus::Variant(std::vector<std::uint32_t>{static_cast<std::uint32_t>(pid)})));
                properties.emplace_back(sdbus::make_struct("Description", sdbus::Variant(std::format("Quiver Container {}", m_container_id))));
                properties.emplace_back(sdbus::make_struct("Delegate", sdbus::Variant(true)));

                std::vector<sdbus::Struct<std::string, std::vector<SystemdProperty>>> aux{};
                systemd_proxy->callMethod("StartTransientUnit")
                             .onInterface("org.freedesktop.systemd1.Manager")
                             .withArguments(unit_name, "fail", properties, aux);

        }
        catch (const sdbus::Error& e) {
                throw std::runtime_error(std::format("Systemd DBus Error: -> {}", e.getMessage()));
        }
}

auto SystemdCGroupsManager::set_cpu_limit(int quota, std::uint64_t period) -> void {
        if (quota <= 0) {
                update_dbus_property("CPUQuotaPerSecUSec", sdbus::Variant(static_cast<uint64_t>(-1)));
        }
        else {
                double percentage{static_cast<double>(quota) / static_cast<double>(period)};
                std::uint64_t systemd_quota{static_cast<std::uint64_t>(percentage * 1000000.0)};

                update_dbus_property("CPUQuotaPerSecUSec", sdbus::Variant(systemd_quota));
                update_dbus_property("CPUQuotaPeriodUSec", sdbus::Variant(period));
        }
}

auto SystemdCGroupsManager::set_cpu_weight(std::uint64_t weight) -> void {
        if (weight < 1) weight = 1;
        if (weight > 10000) weight = 10000;

        update_dbus_property("CPUWeight", sdbus::Variant(static_cast<std::uint64_t>(weight)));
}

auto SystemdCGroupsManager::set_memory_max(std::uint64_t limit_bytes) -> void {
        if (limit_bytes == 0) {
                update_dbus_property("MemoryMax", sdbus::Variant(static_cast<uint64_t>(-1)));
        }
        else {
                update_dbus_property("MemoryMax", sdbus::Variant(limit_bytes));
        }
}

auto SystemdCGroupsManager::set_memory_swap(std::uint64_t limit_bytes) -> void {
        if (limit_bytes == 0) {
                update_dbus_property("MemorySwapMax", sdbus::Variant(static_cast<uint64_t>(-1)));
        }
        else {
                update_dbus_property("MemorySwapMax", sdbus::Variant(limit_bytes));
        }
}

auto SystemdCGroupsManager::set_io_max(std::uint64_t major, std::uint64_t minor, const IOLimits& io_limits) -> void {
        std::string dev_path{std::format("/dev/block/{}:{}", major, minor)};

        auto make_io_variant{[&](std::uint64_t limit) {
                std::vector<sdbus::Struct<std::string, std::uint64_t>> io_array{};
                io_array.emplace_back(sdbus::make_struct(dev_path, limit));
                return sdbus::Variant(io_array);
        }};

        if (io_limits.rbps > 0) {
                update_dbus_property("IOReadBandwidthMax", make_io_variant(io_limits.rbps));
        }

        if (io_limits.wbps > 0) {
                update_dbus_property("IOWriteBandwidthMax", make_io_variant(io_limits.wbps));
        }

        if (io_limits.riops > 0) {
                update_dbus_property("IOReadIOPSMax", make_io_variant(io_limits.riops));
        }

        if (io_limits.wiops > 0) {
                update_dbus_property("IOWriteIOPSMax", make_io_variant(io_limits.wiops));
        }
}

auto SystemdCGroupsManager::set_io_weight(std::uint64_t major, std::uint64_t minor, std::uint64_t weight) -> void {
        if (weight < 1) weight = 1;
        if (weight > 10000) weight = 10000;

        std::string dev_path{std::format("/dev/block/{}:{}", major, minor)};

        std::vector<sdbus::Struct<std::string, std::uint64_t>> io_weight_array{};
        io_weight_array.emplace_back(sdbus::make_struct(dev_path, weight));

        update_dbus_property("IODeviceWeight", sdbus::Variant(io_weight_array));
}

auto SystemdCGroupsManager::set_pid_limit(uint64_t max_pids) -> void {
        if (max_pids == 0) {
                update_dbus_property("TasksMax", sdbus::Variant(static_cast<uint64_t>(-1)));
        }
        else {
                update_dbus_property("TasksMax", sdbus::Variant(max_pids));
        }
}

auto SystemdCGroupsManager::set_cpuset_cpus(const std::string& cpus) -> void {
        if (cpus.empty()) return;

        std::vector<uint8_t> cpu_mask{};

        std::stringstream ss(cpus);
        std::string token{};

        try {
                while (std::getline(ss, token, ',')) {
                        auto dash_pos{token.find('-')};

                        if (dash_pos != std::string::npos) {
                                int start{std::stoi(token.substr(0, dash_pos))};
                                int end{std::stoi(token.substr(dash_pos + 1))};

                                for (int i{start}; i <= end; ++i) {
                                        if (cpu_mask.size() <= (i / 8)) cpu_mask.resize((i / 8) + 1, 0);
                                        cpu_mask[i / 8] |= (1 << (i % 8));
                                }
                        }
                        else {
                                int cpu{std::stoi(token)};
                                if (cpu_mask.size() <= (cpu / 8)) cpu_mask.resize((cpu / 8) + 1, 0);
                                cpu_mask[cpu / 8] |= (1 << (cpu % 8));
                        }
                }

                update_dbus_property("AllowedCPUs", sdbus::Variant(cpu_mask));

        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to parse cpuset.cpus string '{}' -> {}", cpus, e.what()));
        }
}

auto SystemdCGroupsManager::set_cpuset_mems(const std::string& mems) -> void {
        if (mems.empty()) return;

        std::vector<uint8_t> mems_mask{};
        std::stringstream ss(mems);
        std::string token{};

        try {
                while (std::getline(ss, token, ',')) {
                        auto dash_pos = token.find('-');
                        if (dash_pos != std::string::npos) {
                                int start{std::stoi(token.substr(0, dash_pos))};
                                int end{std::stoi(token.substr(dash_pos + 1))};

                                for (int i = start; i <= end; ++i) {
                                        if (mems_mask.size() <= (i / 8)) mems_mask.resize((i / 8) + 1, 0);
                                        mems_mask[i / 8] |= (1 << (i % 8));
                                }
                        }
                        else {
                                int node{std::stoi(token)};
                                if (mems_mask.size() <= (node / 8)) mems_mask.resize((node / 8) + 1, 0);
                                mems_mask[node / 8] |= (1 << (node % 8));
                        }
                }

                update_dbus_property("AllowedMemoryNodes", sdbus::Variant(mems_mask));

        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to parse cpuset.mems string '{}' -> {}", mems, e.what()));
        }
}

auto SystemdCGroupsManager::set_freeze(const std::string& bit) -> void {
        const std::string unit_name{std::format("quiver-{}.scope", m_container_id)};

        try {
                auto connection{sdbus::createSessionBusConnection()};
                auto systemd_proxy{sdbus::createProxy(
                        *connection,
                        sdbus::ServiceName{"org.freedesktop.systemd1"},
                        sdbus::ObjectPath{"/org/freedesktop/systemd1"}
                )};

                if (bit == "1") {
                        systemd_proxy->callMethod(sdbus::MethodName{"FreezeUnit"})
                                     .onInterface(sdbus::InterfaceName{"org.freedesktop.systemd1.Manager"})
                                     .withArguments(unit_name);
                }
                else if (bit == "0") {
                        systemd_proxy->callMethod(sdbus::MethodName{"ThawUnit"})
                                     .onInterface(sdbus::InterfaceName{"org.freedesktop.systemd1.Manager"})
                                     .withArguments(unit_name);
                }
                else {
                        throw std::invalid_argument(std::format("Invalid freeze bit '{}'. Expected '1' or '0'.", bit));
                }

        }
        catch (const sdbus::Error& e) {
                throw std::runtime_error(std::format("CGroups Manager Error: DBus Freeze/Thaw failed -> {}", e.getMessage()));
        }
}

auto SystemdCGroupsManager::update_dbus_property(const std::string& property_name, const sdbus::Variant& value) -> void {
        const std::string unit_name{std::format("quiver-{}.scope", m_container_id)};

        try {
                auto connection{sdbus::createSessionBusConnection()};
                auto systemd_proxy{sdbus::createProxy(*connection, sdbus::ServiceName{"org.freedesktop.systemd1"}, sdbus::ObjectPath("/org/freedesktop/systemd1"))};

                std::vector<SystemdProperty> properties{};
                properties.emplace_back(sdbus::make_struct(property_name, value));

                systemd_proxy->callMethod("SetUnitProperties")
                             .onInterface("org.freedesktop.systemd1.Manager")
                             .withArguments(unit_name, true, properties);

        }
        catch (const sdbus::Error& e) {
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set {} -> {}",
                                                     property_name, e.getMessage()));
        }
}
