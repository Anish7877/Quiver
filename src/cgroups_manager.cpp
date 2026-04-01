#include "cgroups_manager.hpp"
#include "utils.hpp"
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

CGroupsManager::CGroupsManager(const std::string& container_id) : m_container_id{container_id} {
        m_cgroups_path = resolve_cgroups_path() / std::format("quiver_{}", m_container_id);
        Utils::ensure_dir(m_cgroups_path);
}

auto CGroupsManager::attach_process(pid_t pid) -> void {
        write_cgroups_file("cgroup.procs", std::to_string(pid));
}

auto CGroupsManager::set_cpu_limit(int quota, uint64_t period) -> void {
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
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set CPU limit. Is CPU delegation enabled? -> {}", e.what()));
        }
}

auto CGroupsManager::set_cpu_weight(uint64_t weight) -> void {
        if (weight < 1) weight = 1;
        if (weight > 10000) weight = 10000;

        try {
                write_cgroups_file("cpu.weight", std::to_string(weight));
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set CPU weight. -> {}", e.what()));
        }
}

auto CGroupsManager::set_memory_max(uint64_t limit_bytes) -> void {
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
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set memory limit -> {}", e.what()));
        }
}

auto CGroupsManager::set_memory_swap(uint64_t limit_bytes) -> void {
        std::string value{};
        if (limit_bytes <= 0) {
                value = "max";
        } else {
                value = std::to_string(limit_bytes);
        }

        try {
                write_cgroups_file("memory.swap.max", value);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set memory limit -> {}", e.what()));
        }
}

auto CGroupsManager::set_io_max(uint64_t major, uint64_t minor, const IOLimits& io_limits) -> void {
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
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set IO limit -> {}", e.what()));
        }
}

auto CGroupsManager::set_io_weight(uint64_t major, uint64_t minor, uint64_t weight) -> void {
        if (weight < 0) weight = 1;
        if (weight > 10000) weight = 10000;

        std::string value{std::format("{}:{} {}", major, minor, weight)};

        try {
                write_cgroups_file("io.weight", value);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set IO weight -> {}", e.what()));
        }
}

auto CGroupsManager::set_pid_limit(uint64_t max_pids) -> void {
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
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set pids limit -> {}", e.what()));
        }
}

auto CGroupsManager::set_cpuset_cpus(const std::string& cpus) -> void {
        if (cpus.empty()) return;

        try {
                write_cgroups_file("cpuset.cpus", cpus);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("CGroups Manager Error: Failed to set cpuset.cpus -> {}", e.what()));
        }
}

auto CGroupsManager::set_cpuset_mems(const std::string& mems) -> void {
        if (mems.empty()) return;

        try {
                write_cgroups_file("cpuset.mems", mems);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("CGroup Manager Error: Failed to set cpuset.mems -> {}", e.what()));
        }
}

auto CGroupsManager::set_freeze(const std::string& bit) -> void {
        try {
                write_cgroups_file("cgroup.freeze", bit);
        }
        catch (const std::exception& e) {
                throw std::runtime_error(std::format("CGroup Manager Error: Failed to set freeze bit -> {}", e.what()));
        }
}

auto CGroupsManager::write_cgroups_file(const std::string& filename, const std::string& value) -> void {
        fs::path target_path{m_cgroups_path / filename};

        Utils::ensure_file(target_path);
        Utils::write_file(target_path, value);
}

auto CGroupsManager::resolve_cgroups_path() -> fs::path {
        std::ifstream file{"/proc/self/cgroup"};
        if (!file.is_open()) {
                throw std::runtime_error("CGroups Manager Error: Cannot open /proc/self/cgroup.");
        }

        std::string line{};
        std::getline(file, line);

        auto pos{line.find("0::")};
        if (pos == std::string::npos) {
                throw std::runtime_error("CGroups Manager Error: CGroups v2 is not being used.");
        }

        std::string cgroups_suffix{line.substr(pos+3)};
        return fs::path("/sys/fs/cgroup") / fs::relative(cgroups_suffix, "/");
}

CGroupsManager::~CGroupsManager() {
        if (m_should_destroy && !m_cgroups_path.empty()) {
                try {
                        Utils::remove_directory(m_cgroups_path);
                }
                catch (const std::exception& e) {
                        std::cerr << e.what() << '\n';
                }
        }
}
