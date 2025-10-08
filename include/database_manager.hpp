#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>

// Represents the Container Table's Schema
struct Container {
    std::string id;
    std::string name;
    std::string image;
    pid_t pid;
    std::string status;
    std::string created_at;
};

// Represents the Volume Table's Schema
struct Volume {
    int id;
    std::string container_id;
    std::string host_path;
    std::string container_path;
};

class DatabaseManager {
public:
    // Constructor that takes the path to the database file
    explicit DatabaseManager(const std::string& db_path);
    
    // Destructor to close the database connection
    ~DatabaseManager();

    // Initializes the database and creates tables if they don't exist
    bool init_db();

    // Container-related operations
    bool add_container(const Container& container);
    Container get_container(const std::string& container_id);
    bool update_container_status(const std::string& container_id, const std::string& status);
    bool remove_container(const std::string& container_id);
    std::vector<Container> list_containers();

    // Volume-related operations
    bool add_volume(const Volume& volume);
    std::vector<Volume> get_container_volumes(const std::string& container_id);
    bool remove_volume(int volume_id);

private:
    sqlite3* m_db;
    std::string m_db_path;

    // A generic callback function for sqlite3_exec
    static int exec_callback(void* data, int argc, char** argv, char** azColName);
};