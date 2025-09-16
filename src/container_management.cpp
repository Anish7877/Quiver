#include "../include/container_management.hpp"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <sys/stat.h>
#include <ctime>
#include <cstdlib>

// Helper function to enable WAL mode, which helps prevent locking issues with fork().
static bool set_wal_mode(sqlite3* db, std::string& error) {
    char* errmsg = nullptr;
    if (sqlite3_exec(db, "PRAGMA journal_mode=WAL;", 0, 0, &errmsg) != SQLITE_OK) {
        error = "Failed to set WAL mode: " + std::string(errmsg);
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

// The constructor opens the database and keeps the connection open.
ContainerManager::ContainerManager() : m_db(nullptr) {
    const char* home_dir = getenv("HOME");
    if (home_dir) {
        std::string quiver_dir = std::string(home_dir) + "/.quiver";
        mkdir(quiver_dir.c_str(), 0755); // Ensure the directory exists.
        m_db_path = quiver_dir + "/quiver.db";
    } else {
        m_db_path = "/tmp/quiver.db"; // Fallback for safety.
    }
    
    // Open the database connection.
    if (sqlite3_open(m_db_path.c_str(), &m_db) != SQLITE_OK) {
        std::cerr << "CRITICAL: Cannot open database: " << sqlite3_errmsg(m_db) << std::endl;
        exit(1); // Exit if the database cannot be opened.
    }
    
    std::string error;
    if (!init_database(error)) {
        std::cerr << "CRITICAL: Database initialization failed: " << error << std::endl;
        exit(1);
    }
}

// The destructor closes the persistent database connection.
ContainerManager::~ContainerManager() {
    if (m_db) {
        sqlite3_close(m_db);
    }
}

// Initializes the database schema if it doesn't already exist.
bool ContainerManager::init_database(std::string& error) {
    if (!set_wal_mode(m_db, error)) {
        return false;
    }
    
    // Tell SQLite to wait up to 1000ms if the database is locked.
    sqlite3_busy_timeout(m_db, 1000);

    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS containers ("
        "id TEXT PRIMARY KEY NOT NULL, name TEXT NOT NULL, image TEXT NOT NULL, "
        "status TEXT NOT NULL, pid INTEGER, created_at TEXT NOT NULL);";

    char* errmsg = nullptr;
    if (sqlite3_exec(m_db, create_table_sql, 0, 0, &errmsg) != SQLITE_OK) {
        error = "Failed to create table: " + std::string(errmsg);
        sqlite3_free(errmsg);
        return false;
    }
    
    return true;
}

// Generates a random 12-character hex string for the container ID.
std::string ContainerManager::generate_container_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 15);
    std::stringstream ss;
    for (int i = 0; i < 12; ++i) {
        ss << std::hex << distrib(gen);
    }
    return ss.str();
}

// Creates a new container entry in the database with "created" status.
std::string ContainerManager::create_container(const std::string& image_name, std::string& error) {
    std::string id = generate_container_id();
    std::string name = "quiver_" + id.substr(0, 6);
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    char time_buf[20];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
    std::string created_at = time_buf;

    const char* sql = "INSERT INTO containers (id, name, image, status, pid, created_at) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK) {
        error = sqlite3_errmsg(m_db);
        return "";
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, image_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, "created", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, -1); // PID is -1 until the container is started.
    sqlite3_bind_text(stmt, 6, created_at.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        return "";
    }

    sqlite3_finalize(stmt);
    return id;
}

// Updates a container's status to "running" and records its PID.
bool ContainerManager::start_container(const std::string& id, pid_t pid, std::string& error) {
    const char* sql = "UPDATE containers SET status = ?, pid = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK) {
        error = sqlite3_errmsg(m_db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, "running", -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, static_cast<int>(pid));
    sqlite3_bind_text(stmt, 3, id.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        error = sqlite3_errmsg(m_db);
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

// Retrieves a list of all containers from the database.
std::vector<Container> ContainerManager::list_containers(std::string& error) {
    std::vector<Container> containers;
    const char* sql = "SELECT id, name, image, status, pid, created_at FROM containers;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK) {
        error = sqlite3_errmsg(m_db);
        return containers;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Container c;
        c.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        c.image = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        c.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        c.pid = static_cast<pid_t>(sqlite3_column_int(stmt, 4));
        c.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        containers.push_back(c);
    }

    sqlite3_finalize(stmt);
    return containers;
}

// Implementation of the new lookup function.
pid_t ContainerManager::get_container_pid(const std::string& id_or_name, std::string& error) {
    const char* sql = "SELECT pid FROM containers WHERE id = ? OR name = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, 0) != SQLITE_OK) {
        error = sqlite3_errmsg(m_db);
        return -1;
    }

    // Bind the user's input to both the ID and name placeholders.
    sqlite3_bind_text(stmt, 1, id_or_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, id_or_name.c_str(), -1, SQLITE_STATIC);

    pid_t pid = -1;
    // Check if a row was returned.
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        pid = static_cast<pid_t>(sqlite3_column_int(stmt, 0));
    } else {
        // If no row is found, set an appropriate error message.
        error = "Container not found: " + id_or_name;
    }

    sqlite3_finalize(stmt);
    return pid;
}

