#include "../include/package_manager.hpp"
#include "../include/utils.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

int PackageManager::initialize(){
    // Pre-create common directories that package managers need with proper permissions
    std::vector<std::string> common_dirs = {
        "/var",
        "/var/cache",
        "/var/log",
        "/var/lib",
        "/usr/share",
        "/usr/share/doc",
        "/usr/share/man",
        "/usr/share/bug",
        "/usr/share/lintian",
        "/usr/share/pixmaps",
        "/usr/share/applications",
        "/etc"
    };

    for (const auto& dir : common_dirs) {
        if (mkdir(dir.c_str(), 0755) == -1 && errno != EEXIST) {
            std::cerr << "Warning: Could not create " << dir << ": " << strerror(errno) << '\n';
        }
        chmod(dir.c_str(), 0755);  // Ensure proper permissions even if exists
    }

    Managers pkg { get_manager() };

    if (pkg == Managers::unknown) {
        std::cerr << "Warning: Could not detect package manager, skipping configuration" << '\n';
        return 0;  // Don't fail, just warn
    }
    switch (pkg) {
        case Managers::apk : {
            mkdir("/etc/apk", 0755);
            chmod("/etc/apk", 0755);
            std::ofstream apk_conf("/etc/apk/apk.conf");
            if (apk_conf.is_open()){
            apk_conf << "# Rootless container APK configuration\n";
            apk_conf << "@testing http://dl-cdn.alpinelinux.org/alpine/edge/testing\n";
            apk_conf << "@community http://dl-cdn.alpinelinux.org/alpine/edge/community\n";
            apk_conf.close();
            std::cerr << "DEBUG: APK configuration created" << '\n';
            }
        }
        break;
        case Managers::apt : {
            // Create APT directories
            std::vector<std::string> apt_dirs = {
                "/etc/apt",
                "/etc/apt/apt.conf.d",
                "/etc/apt/sources.list.d",
                "/etc/apt/preferences.d",
                "/var/lib/apt",
                "/var/lib/apt/lists",
                "/var/lib/apt/lists/partial",
                "/var/cache/apt",
                "/var/cache/apt/archives",
                "/var/cache/apt/archives/partial",
                "/var/log/apt"
            };

            for (const auto& dir : apt_dirs) {
                mkdir(dir.c_str(), 0755);
                chmod(dir.c_str(), 0755);
            }

            std::ofstream apt_conf("/etc/apt/apt.conf.d/99-rootless-container");
            if (apt_conf.is_open()) {
                apt_conf << "// Rootless container APT configuration\n";
                apt_conf << "APT::Sandbox::User \"root\";\n";
                apt_conf << "APT::Get::Assume-Yes \"true\";\n";
                apt_conf << "APT::Install-Recommends \"false\";\n";
                apt_conf << "APT::Install-Suggests \"false\";\n";
                apt_conf << "Dpkg::Use-Pty \"0\";\n";
                apt_conf << "Debug::NoLocking \"true\";\n";
                apt_conf << "Acquire::Retries \"3\";\n";
                apt_conf << "Acquire::Check-Valid-Until \"false\";\n";
                apt_conf << "APT::Get::Fix-Broken \"true\";\n";
                apt_conf.close();
                std::cerr << "DEBUG: APT configuration created" << '\n';
            }

            // DPKG configuration and directories
            std::vector<std::string> dpkg_dirs = {
                "/etc/dpkg",
                "/etc/dpkg/dpkg.cfg.d",
                "/var/lib/dpkg",
                "/var/lib/dpkg/info",
                "/var/lib/dpkg/updates",
                "/var/lib/dpkg/triggers"
            };

            for (const auto& dir : dpkg_dirs) {
                mkdir(dir.c_str(), 0755);
                chmod(dir.c_str(), 0755);
            }

            std::ofstream dpkg_conf("/etc/dpkg/dpkg.cfg.d/99-rootless-container");
            if (dpkg_conf.is_open()) {
                dpkg_conf << "# Rootless container DPKG configuration\n";
                dpkg_conf << "force-unsafe-io\n";
                dpkg_conf << "no-debsig\n";
                dpkg_conf << "force-depends\n";
                dpkg_conf << "force-confnew\n";
                dpkg_conf << "force-overwrite\n";  // Allow overwriting files
                dpkg_conf.close();
                std::cerr << "DEBUG: DPKG configuration created" << '\n';
            }
        }
        break;
        case Managers::pacman : {
            std::vector<std::string> pacman_dirs = {
                "/etc/pacman.d",
                "/var/lib/pacman",
                "/var/cache/pacman",
                "/var/cache/pacman/pkg",
                "/var/log"
            };

            for (const auto& dir : pacman_dirs) {
                mkdir(dir.c_str(), 0755);
                chmod(dir.c_str(), 0755);
            }

            std::ofstream pacman_conf("/etc/pacman.conf");
            if (pacman_conf.is_open()) {
                pacman_conf << "# Rootless container Pacman configuration\n";
                pacman_conf << "[options]\n";
                pacman_conf << "RootDir     = /\n";
                pacman_conf << "DBPath      = /var/lib/pacman/\n";
                pacman_conf << "CacheDir    = /var/cache/pacman/pkg/\n";
                pacman_conf << "LogFile     = /var/log/pacman.log\n";
                pacman_conf << "GPGDir      = /etc/pacman.d/gnupg/\n";
                pacman_conf << "HookDir     = /etc/pacman.d/hooks/\n";
                pacman_conf << "HoldPkg     = pacman glibc\n";
                pacman_conf << "CleanMethod = KeepInstalled\n";
                pacman_conf << "Architecture = auto\n";
                pacman_conf << "Color\n";
                pacman_conf << "VerbosePkgLists\n";
                pacman_conf << "NoUpgrade   = etc/passwd etc/group etc/shadow\n";
                pacman_conf << "NoExtract   = usr/share/man/* usr/share/doc/*\n";
                pacman_conf << "SigLevel    = Never\n";
                pacman_conf << "\n[core]\n";
                pacman_conf << "Include = /etc/pacman.d/mirrorlist\n";
                pacman_conf << "\n[extra]\n";
                pacman_conf << "Include = /etc/pacman.d/mirrorlist\n";
                pacman_conf.close();
                std::cerr << "DEBUG: Pacman configuration created" << '\n';
            }
        }
        break;
        case Managers::rcf : {
            std::vector<std::string> yum_dirs = {
                "/etc/yum",
                "/etc/dnf",
                "/etc/rpm",
                "/var/lib/rpm",
                "/var/lib/dnf",
                "/var/cache/yum",
                "/var/cache/dnf",
                "/var/log"
            };

            for (const auto& dir : yum_dirs) {
                mkdir(dir.c_str(), 0755);
                chmod(dir.c_str(), 0755);
            }

            std::ofstream yum_conf("/etc/yum.conf");
            if (yum_conf.is_open()) {
                yum_conf << "[main]\n";
                yum_conf << "# Rootless container YUM configuration\n";
                yum_conf << "assumeyes=1\n";
                yum_conf << "keepcache=0\n";
                yum_conf << "debuglevel=2\n";
                yum_conf << "logfile=/var/log/yum.log\n";
                yum_conf << "exactarch=1\n";
                yum_conf << "obsoletes=1\n";
                yum_conf << "gpgcheck=1\n";
                yum_conf << "plugins=1\n";
                yum_conf << "installonly_limit=3\n";
                yum_conf << "tsflags=nodocs\n";
                yum_conf.close();
                std::cerr << "DEBUG: YUM configuration created" << '\n';
            }

            // DNF configuration
            std::ofstream dnf_conf("/etc/dnf/dnf.conf");
            if (dnf_conf.is_open()) {
                dnf_conf << "[main]\n";
                dnf_conf << "# Rootless container DNF configuration\n";
                dnf_conf << "assumeyes=True\n";
                dnf_conf << "keepcache=False\n";
                dnf_conf << "debuglevel=2\n";
                dnf_conf << "logfile=/var/log/dnf.log\n";
                dnf_conf << "exactarch=True\n";
                dnf_conf << "obsoletes=True\n";
                dnf_conf << "gpgcheck=True\n";
                dnf_conf << "plugins=True\n";
                dnf_conf << "installonly_limit=3\n";
                dnf_conf << "tsflags=nodocs\n";
                dnf_conf << "clean_requirements_on_remove=True\n";
                dnf_conf.close();
                std::cerr << "DEBUG: DNF configuration created" << '\n';
            }

            // RPM configuration
            std::ofstream rpmrc("/etc/rpm/macros.rootless");
            if (rpmrc.is_open()) {
                rpmrc << "# Rootless container RPM macros\n";
                rpmrc << "%_var                   /var\n";
                rpmrc << "%_usr                   /usr\n";
                rpmrc << "%_rpmlock_path          %{_var}/lock/rpm/transaction\n";
                rpmrc << "%__dbi_htconfig         hash nofsync\n";
                rpmrc.close();
                std::cerr << "DEBUG: RPM configuration created" << '\n';
            }
        }
        break;
        case Managers::zypper : {
            std::vector<std::string> zypper_dirs = {
                "/etc/zypp",
                "/var/cache/zypp",
                "/var/cache/zypp/packages",
                "/var/cache/zypp/raw",
                "/var/cache/zypp/solv"
            };

            for (const auto& dir : zypper_dirs) {
                mkdir(dir.c_str(), 0755);
                chmod(dir.c_str(), 0755);
            }

            std::ofstream zypper_conf("/etc/zypp/zypp.conf");
            if (zypper_conf.is_open()) {
                zypper_conf << "# Rootless container Zypper configuration\n";
                zypper_conf << "[main]\n";
                zypper_conf << "solver.onlyRequires = true\n";
                zypper_conf << "solver.installRecommends = false\n";
                zypper_conf << "solver.installSuggests = false\n";
                zypper_conf << "solver.allowVendorChange = true\n";
                zypper_conf << "multiversion = provides:multiversion(kernel)\n";
                zypper_conf << "multiversion.kernels = latest,latest-1,running\n";
                zypper_conf << "commit.downloadMode = DownloadInAdvance\n";
                zypper_conf << "pkg-cache-dir = /var/cache/zypp/packages\n";
                zypper_conf << "metadata-cache-dir = /var/cache/zypp/raw\n";
                zypper_conf << "solv-cache-dir = /var/cache/zypp/solv\n";
                zypper_conf << "packages-cache-dir = /var/cache/zypp/packages\n";
                zypper_conf.close();
                std::cerr << "DEBUG: Zypper configuration created" << '\n';
            }
        }
        break;
        default : return -1;
    }
    return 0;
}

PackageManager::Managers PackageManager::get_manager(){
    std::string os_release {  "/etc/os-release" };
    if (Utils::path_exists(os_release)) {
        std::ifstream f(os_release);
        std::string line{}, content{};
        while (std::getline(f, line)) {
            std::transform(line.begin(), line.end(), line.begin(), ::tolower);
            content += line + "\n";
        }
        if (content.find("id=alpine") != std::string::npos)
            return Managers::apk;
        if (content.find("id=debian") != std::string::npos || content.find("id=ubuntu") != std::string::npos)
            return Managers::apt;
        if (content.find("id=fedora") != std::string::npos ||
            content.find("id=centos") != std::string::npos ||
            content.find("id=rhel")   != std::string::npos)
            return Managers::rcf;
        if (content.find("id=arch") != std::string::npos)
            return Managers::pacman;
        if (content.find("id=opensuse") != std::string::npos || content.find("id=sles") != std::string::npos)
            return Managers::zypper; }

    // 2. Look for known package manager binaries
    std::vector<std::pair<Managers, std::vector<std::string>>> bin_markers{ {
        {Managers::apk, {"apk"}},
        {Managers::apt, {"apt-get", "apt", "dpkg"}},
        {Managers::rcf, {"rcf", "dnf", "rpm"}},
        {Managers::pacman, {"pacman"}},
        {Managers::zypper, {"zypper"}}
    }};

    std::vector<std::string> bin_dirs { {
        "/usr/bin",
        "/bin"
    } };

    for (auto& [pkg, bins] : bin_markers) {
        for (auto& b : bins) {
            for (auto& d : bin_dirs) {
                std::string full = d + "/" + b;
                if (Utils::path_exists(full)) {
                    return pkg;
                }
            }
        }
    }

    // 3. Check package database directories under /var/lib
    std::string lib_dir { "/var/lib" };
    DIR* dir { opendir(lib_dir.c_str()) }; if (dir) {
        dirent* entry{};
        while ((entry = readdir(dir)) != nullptr) {
            std::string name { entry->d_name };
            if (name == "." || name == "..") continue;
            if (name == "dpkg" || name == "apt") { closedir(dir); return Managers::apt; }
            if (name == "apk")  { closedir(dir); return Managers::apk; }
            if (name == "rpm" || name == "rcf" || name == "dnf")  { closedir(dir); return Managers::rcf; }
            if (name == "pacman") { closedir(dir); return Managers::pacman; }
            if (name == "zypp") { closedir(dir); return Managers::zypper; }
        }
        closedir(dir);
    }

    return Managers::unknown;
}
