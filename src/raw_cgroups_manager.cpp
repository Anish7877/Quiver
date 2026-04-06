#include "raw_cgroups_manager.hpp"
#include "utils.hpp"
#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

RawCGroupsManager::RawCGroupsManager(const std::string& container_id, const fs::path& delegated_path) : m_container_id{container_id} {
        m_cgroups_path = resolve_cgroups_path(delegated_path) / std::format("quiver_{}", m_container_id);
        Utils::ensure_dir(m_cgroups_path);
}

auto RawCGroupsManager::attach_process(pid_t pid) -> void {
        write_cgroups_file("cgroup.procs", std::to_string(pid));
}

auto RawCGroupsManager::set_cpu_limit(int quota, uint64_t period) -> void {
        std::string value{};
        if (quota <= 0) {
                value = std::format("max {}", period);
        }
        else {
                value = std::format("{} {}", quota, period);
        }

        try {
                write_cgroups_file("cpu.max", value);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set CPU limit. Is CPU delegation enabled? -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_cpu_weight(uint64_t weight) -> void {
        if (weight < 1) weight = 1;
        if (weight > 10000) weight = 10000;

        try {
                write_cgroups_file("cpu.weight", std::to_string(weight));
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set CPU weight. -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_memory_max(uint64_t limit_bytes) -> void {
        std::string value{};
        if (limit_bytes <= 0) {
                value = "max";
        } else {
                value = std::to_string(limit_bytes);
        }

        try {
                write_cgroups_file("memory.max", value);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set memory limit -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_memory_swap(uint64_t limit_bytes) -> void {
        std::string value{};
        if (limit_bytes <= 0) {
                value = "max";
        }
        else {
                value = std::to_string(limit_bytes);
        }

        try {
                write_cgroups_file("memory.swap.max", value);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set memory limit -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_io_max(uint64_t major, uint64_t minor, const IOLimits& io_limits) -> void {
        std::string value{std::format("{}:{}", major, minor)};

        if (io_limits.rbps > 0) {
                value += std::format(" rbps={}", io_limits.rbps);
        }
        if (io_limits.wbps > 0) {
                value += std::format(" wbps={}", io_limits.rbps);
        }
        if (io_limits.riops > 0) {
                value += std::format(" riops={}", io_limits.rbps);
        }
        if (io_limits.wiops > 0) {
                value += std::format(" wiops={}", io_limits.rbps);
        }

        try {
                write_cgroups_file("io.max", value);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set IO limit -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_io_weight(uint64_t major, uint64_t minor, uint64_t weight) -> void {
        if (weight < 0) weight = 1;
        if (weight > 10000) weight = 10000;

        std::string value{std::format("{}:{} {}", major, minor, weight)};

        try {
                write_cgroups_file("io.weight", value);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set IO weight -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_pid_limit(uint64_t max_pids) -> void {
        std::string value{};
        if (max_pids <= 0) {
                value = "max";
        }
        else {
                value = std::to_string(max_pids);
        }
        try {
                write_cgroups_file("pids.max", value);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set pids limit -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_cpuset_cpus(const std::string& cpus) -> void {
        if (cpus.empty()) return;

        try {
                write_cgroups_file("cpuset.cpus", cpus);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set cpuset.cpus -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_cpuset_mems(const std::string& mems) -> void {
        if (mems.empty()) return;

        try {
                write_cgroups_file("cpuset.mems", mems);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set cpuset.mems -> {}", e.what()));
        }
}

auto RawCGroupsManager::set_freeze(const std::string& bit) -> void {
        try {
                write_cgroups_file("cgroup.freeze", bit);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("RawCGroups Manager Error: Failed to set freeze bit -> {}", e.what()));
        }
}

auto RawCGroupsManager::write_cgroups_file(const std::string& filename, const std::string& value) -> void {
        fs::path target_path{m_cgroups_path / filename};
        Utils::write_file(target_path, value);
}


auto RawCGroupsManager::resolve_cgroups_path(const fs::path& delegated_path) -> fs::path {
        if (geteuid() == 0) {
                std::ifstream file{"/proc/self/cgroup"};
                if (!file.is_open()) {
                        throw std::runtime_error("RawCGroups Manager Error: Cannot open /proc/self/cgroup.");
                }
                std::string line{};
                std::getline(file, line);
                auto pos{line.find("0::")};
                if (pos == std::string::npos) {
                        throw std::runtime_error("RawCGroups Manager Error: CGroups v2 is not being used.");
                }
                std::string cgroups_suffix{line.substr(pos+3)};
                return fs::path("/sys/fs/cgroup") / fs::relative(cgroups_suffix, "/");
        }
        else {
                if (!fs::exists(delegated_path)) {
                        throw std::runtime_error(std::format(
                                                "RawCgroups Manager Error: The directory {} does not exist. "
                                                "Because systemd is not present, your system administrator must manually "
                                                "create this directory and grant your user ownership of it to use resource limits.",
                                                delegated_path.string()
                                                ));
                }
                return delegated_path;
        }
}

RawCGroupsManager::~RawCGroupsManager() {
        if (!m_cgroups_path.empty()) {
                try {
                        Utils::remove_directory(m_cgroups_path);
                }
                catch (const std::exception& e) {
                        std::cerr << e.what() << '\n';
                }
        }
}
