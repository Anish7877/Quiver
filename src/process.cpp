#include "../include/process.hpp"
#include "../include/tty_proxy_server.hpp"
#include "../include/utils.hpp"
#include "../include/network.hpp"
#include "../include/package_manager.hpp"
#include "../include/device_manager.hpp"
#include "../include/database_manager.hpp"
#include "../include/container_management.hpp"

int Process::start(const std::string& new_hostname,const std::vector<std::string>& volumes,const std::string& filesystem_path,const std::string& path){
    m_new_hostname = new_hostname;
    m_new_fs = filesystem_path;
    m_volumes = volumes;

    // Initialize database and container managers
    std::string db_path = Utils::get_base_dir() + "quiver.db";
    DatabaseManager db(db_path);
    ContainerManager container_manager(db);

    // Create the container record in the database before starting the process
    m_container_id = container_manager.create_container(m_new_hostname);
    if (m_container_id.empty()) {
        std::cerr << "Failed to create container in database." << std::endl;
        return ERR;
    }

    if(run(path, m_container_id) == ERR) return ERR;
    return 0;
}

int Process::run_container(ContainerArgs* args) {
    if(unshare(CLONE_NEWUTS | CLONE_NEWNET | CLONE_NEWNS) != 0) Utils::handle_error("namespace creation error");
    // Setup TTY
    if (setsid() == ERR) {
        Utils::handle_error("setsid error");
    }

    if (sethostname(args->hostname.c_str(), args->hostname.length()) != 0) {
        std::cerr << "Failed to set hostname: " << strerror(errno) << '\n';
        return ERR;
    }

    std::string filesystem_path = args->filesystem_dir;
    std::string upper { filesystem_path + "upper" };
    std::string merged { filesystem_path + "merged" };
    std::string work  { filesystem_path + "work" };

    Utils::ensure_dirs(upper);
    Utils::ensure_dirs(work);
    Utils::ensure_dirs(merged);

    pid_t overlay_pid { fork() };
    if (overlay_pid == 0) {
        const char *fuse_bin { "/usr/bin/fuse-overlayfs" };
        execlp(fuse_bin, fuse_bin,
                "-o", ("lowerdir=" + args->rootfs_path).c_str(),
                "-o", ("upperdir=" + upper).c_str(),
                "-o", ("workdir=" + work).c_str(),
                merged.c_str(),
                (char*) NULL);
        _exit(1);
    }
    waitpid(overlay_pid, NULL, 0);

    // Create necessary directories in new root
    mkdir((merged + "/proc").c_str(), 0755);
    mkdir((merged + "/sys").c_str(), 0755);
    mkdir((merged + "/dev").c_str(), 0755);
    mkdir((merged + "/old_root").c_str(), 0755);
    mkdir((merged + "/etc").c_str(), 0755);

    size_t no_volumes{m_volumes.size()};
    if(no_volumes > 0){
        std::vector<std::string> paths{};
        std::vector<std::string> dirs{};
        for(size_t i{0};i<no_volumes;++i){
            size_t pos{ m_volumes[i].find(':') };
            paths.emplace_back(m_volumes[i].substr(0,pos));
            dirs.emplace_back(m_volumes[i].substr(pos+1));
        }
        for(size_t i{0};i<dirs.size();++i){
            dirs[i] = merged + dirs[i];
            Utils::ensure_dirs(dirs[i]);
        }
        for(size_t i{0};i<paths.size();++i){
            if(mount(paths[i].c_str(),dirs[i].c_str(),nullptr,MS_BIND|MS_REC,nullptr) == ERR){
                std::cerr << "Unable to mount " << paths[i] << " to " << dirs[i] << '\n';
            }
            else{
                std::cout << "successfully mounted " << paths[i] << " to " << dirs[i] << '\n';
            }
        }
    }
    // Pre-mount essential filesystems
    std::string new_proc_path { merged + "/proc" };
    std::string new_sys_path  { merged + "/sys" };
    std::string new_dev_path  { merged + "/dev" };

    mount("proc", new_proc_path.c_str(), "proc", MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL);
    mount("sysfs", new_sys_path.c_str(), "sysfs", MS_NODEV|MS_NOSUID|MS_NOEXEC, NULL);
    mount("tmpfs", new_dev_path.c_str(), "tmpfs", 0, NULL);

    if(mount(merged.c_str(), merged.c_str(), nullptr, MS_BIND | MS_REC, NULL) == ERR) {
        std::cerr << "Unable to mount new fs: " << strerror(errno) << '\n';
        return ERR;
    }

    // Change to merged directory and pivot root
    if (chdir(merged.c_str()) != 0) {
        std::cerr << "chdir to merged failed: " << strerror(errno) << '\n';
        return ERR;
    }

    if (syscall(SYS_pivot_root, ".", "old_root") != 0) {
        std::cerr << "pivot_root failed: " << strerror(errno) << '\n';
        return ERR;
    }

    if (chdir("/") != 0) {
        std::cerr << "chdir to / failed: " << strerror(errno) << '\n';
        return ERR;
    }

    // Unmount old root
    umount2("/old_root", MNT_DETACH);
    rmdir("/old_root");
    std::cerr << "DEBUG: Pivot completed successfully!" << '\n';

    std::ofstream resolv("/etc/resolv.conf");
    resolv << "nameserver 10.0.2.3\n";
    resolv.close();
    // Configure network interface inside container


    if(PackageManager::initialize() == ERR) Utils::handle_error("cannot setup package manager");
    std::cerr << "Starting program: " << args->program_path << '\n';
    execl(args->program_path.c_str(), args->program_path.c_str(), (char*)NULL);

    std::cerr << "exec failed: " << strerror(errno) << '\n';
    return ERR;
}

int Process::run(const std::string& path, std::string& container_id){
    // Create PTY pair
    int master_fd{}, slave_fd{};
    char slave_name[128];
    winsize ws{};
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);

    if (openpty(&master_fd, &slave_fd, slave_name, NULL, &ws) == ERR) {
        Utils::handle_error("openpty failed");
    }
    int namespace_pipe[2];

    // Prepare arguments for container
    std::string filesystem_dir { Utils::get_filesystem_path(getpid()) };
    ContainerArgs args{m_new_hostname, m_new_fs, path, slave_fd, filesystem_dir};

    if(unshare(CLONE_NEWUSER | CLONE_NEWPID) != 0) Utils::handle_error("User namespace creation error");

    if(pipe(namespace_pipe) != 0) Utils::handle_error("Cannot create sync pipe for namespace setup");

    m_child_pid = fork();
    if (m_child_pid == ERR) {
        close(namespace_pipe[0]);
        close(namespace_pipe[1]);
        close(master_fd);
        close(slave_fd);
        Utils::handle_error("fork failed");
    }
    if(m_child_pid == 0){
        close(namespace_pipe[1]);
        char buf{};
        if(read(namespace_pipe[0],&buf,1) != 1) Utils::handle_error("read for namespace pipe failed");
        if(buf == 'r'){
            run_container(&args);
        }
    }
    else{
        try {
            DatabaseManager db(Utils::get_base_dir() + "quiver.db");
            db.update_container_pid(m_container_id, m_child_pid);
            db.update_container_status(m_container_id, "running");
            close(namespace_pipe[0]);
            setup_user_namespace();
            if(write(namespace_pipe[1], "r", 1) != 1) Utils::handle_error("write to sync_pipe failed");

            close(slave_fd);

            std::cerr << "Container started with PID: " << m_child_pid << '\n';

            // Setup user namespace mappings

            if (Network::setup_networking(m_child_pid) != 0) {
                Utils::handle_error("Failed to setup network");
            }

            pid_t proxy_pid { fork() };
            if (proxy_pid == ERR) {
                close(master_fd);
                Utils::handle_error("fork for proxy failed");
            }

            TTYProxyServer tty{};
            if (proxy_pid == 0) {
                std::string sock = Utils::get_sock_path(m_child_pid);
                std::cerr << "DEBUG: tty proxy socket: " << sock << '\n';
                if(tty.start(master_fd, m_child_pid, sock, container_id) == ERR)
                    Utils::handle_error("Proxy server start failed");
                exit(1);
            } else {
                close(master_fd);

                // Wait for container
                int status{};
                waitpid(m_child_pid, &status, WNOHANG);

                // TODO: Update to use container id
                std::string sock { Utils::get_sock_path(m_child_pid) };
                std::cerr << "Container started. attach socket: " << sock << '\n';
                return 0;
            }
        } catch (const std::runtime_error& e) {
            std::cerr << "Database Error in run(): " << e.what() << std::endl;
            return ERR;
        }
    }
    return 0;
}

void Process::setup_user_namespace() {
    Utils::write_file("/proc/self/uid_map", "0 1000 1\n");
    Utils::write_file("/proc/self/setgroups", "deny\n");
    Utils::write_file("/proc/self/gid_map", "0 1000 1\n");
}