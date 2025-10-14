#pragma once
#include <vector>
#include <string>

extern std::vector<std::string> volumes;
extern std::vector<std::string> commands;
extern std::string image_name;
extern std::string container_name;
extern pid_t container_pid;
namespace CommandLineHandler{
    void run(const std::vector<std::string>& cmds);
    void attach(const std::vector<std::string>& cmds);
    void ps(const std::vector<std::string>& cmds);
    void rm(const std::vector<std::string>& cmds);
    void image(const std::vector<std::string>& cmds);
    void volume(const std::vector<std::string>& cmds);
    void create(const std::vector<std::string>& cmds);
    void pull(const std::vector<std::string>& cmds);
    void start(const std::vector<std::string>& cmds);
    void stop(const std::vector<std::string>& cmds);
}
