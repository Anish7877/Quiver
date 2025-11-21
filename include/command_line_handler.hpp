#pragma once
#include <vector>
#include <string>

extern std::vector<std::string> volumes;
extern std::vector<std::string> commands;
extern std::string image_name;
extern std::string container_id;
extern pid_t container_pid;

class CommandLineHandler {
public:
    static void run(DatabaseManager& db, const std::vector<std::string>& cmds);
    static void attach(DatabaseManager& db, const std::vector<std::string>& cmds);
    static void ps(DatabaseManager& db_manager, const std::vector<std::string>& cmds);
    static void rm(DatabaseManager& db_manager, const std::vector<std::string>& cmds);
    static void image(DatabaseManager& db_manager, ImageManager& img_manager, const std::vector<std::string>& cmds);
    static void volume(DatabaseManager& db_manager, const std::vector<std::string>& cmds);
    static void create(DatabaseManager& db_manager, ImageManager& img, const std::vector<std::string>& cmds);
    static void pull(DatabaseManager& db_manager, const std::vector<std::string>& cmds);
    static void start(DatabaseManager& db_manager, const std::vector<std::string>& cmds);
    static void stop(DatabaseManager& db_manager, const std::vector<std::string>& cmds);
};
