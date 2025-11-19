#pragma once
#include <string>
#include <vector>
#include "database_manager.hpp"
#include "terminal.hpp"

class Container{
    public:
        struct ContainerArgs{
            std::string hostname{};
            std::string rootfs_path{};
            std::string program_path{};
            int slave_fd{};
            std::string filesystem_dir{};
            std::vector<std::string> commands{};
        };

        explicit Container() = default;
        explicit Container(const std::string& hostname,
                           const std::string& new_fs,
                           const std::vector<std::string>& volumes,
                           const std::vector<std::pair<int,int>>& ports,
                           const std::string& container_id,
                           DatabaseManager& db, const std::string& image_name);
        ~Container() {};
        void exec(const std::string& program_path, const std::vector<std::string>& commands);
        void run_container(const ContainerArgs& args);
        void set_filesystem(const std::string& path);
        void connect_to_server(const pid_t& container_pid);
    private:
        void manage_container(const std::string& path, const std::string& filesystem_dir);
        void setup_user_namespace();
        void run(const std::string& path, const std::string& container_id);
        DatabaseManager* m_db;
        static pid_t m_child_pid;
        static std::string m_new_fs;
        static std::string m_new_hostname;
        static std::vector<std::string> m_volumes;
        static std::vector<std::string> m_commands;
        static std::vector<std::pair<int,int>> m_forward_ports;
        std::string m_container_id{};
        static Terminal m_term;
        static Terminal::PtyArgs m_pty_args;
        static std::string m_image_name;
};
