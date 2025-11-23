#include "../include/database_manager.hpp"
#include <iostream>
#include <sqlite3.h>
#include <stdexcept>
#include <utility>

DatabaseManager::DatabaseManager(const std::string& db_path) : m_db(nullptr), m_db_path(db_path) {
    if (sqlite3_open(m_db_path.c_str(), &m_db) != SQLITE_OK) {
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(m_db)));
    }
}

DatabaseManager::~DatabaseManager() {
    if (m_db) {
        sqlite3_close(m_db);
    }
}

bool DatabaseManager::init_db() {
    const char* create_containers_table{
        "CREATE TABLE IF NOT EXISTS containers ("
            "id TEXT PRIMARY KEY NOT NULL,"
            "name TEXT NOT NULL UNIQUE,"
            "image TEXT NOT NULL,"
            "pid INTEGER,"
            "net_pid INTEGER,"
            "status TEXT NOT NULL,"
            "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "hostname TEXT,"
            "filesystem_path TEXT,"
            "pty_shell TEXT,"
            "vfs BOOLEAN DEFAULT 0,"
            "no_remove BOOLEAN DEFAULT 0,"
            "vfs_path TEXT"
            ");"
    };

    const char* create_images_table{
        "CREATE TABLE IF NOT EXISTS images ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE,"
        "tag TEXT NOT NULL UNIQUE,"
        "path TEXT NOT NULL,"
        "size INT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
    };
    const char* create_volumes_table{
        "CREATE TABLE IF NOT EXISTS volumes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "container_id TEXT,"
        "host_path TEXT NOT NULL,"
        "container_path TEXT NOT NULL,"
        "FOREIGN KEY(container_id) REFERENCES containers(id),"
        "CONSTRAINT vol_unique UNIQUE(container_id, host_path, container_path)"
        ");"
    };

    const char* create_networks_table{
        "CREATE TABLE IF NOT EXISTS networks ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "container_id TEXT,"
        "host_port INT NOT NULL,"
        "container_port INT NOT NULL,"
        "FOREIGN KEY(container_id) REFERENCES containers(id),"
        "CONSTRAINT net_unique UNIQUE(container_id, host_port, container_port)"
        ");"
    };

    char* err_msg{ nullptr };
    if (sqlite3_exec(m_db, create_containers_table, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << '\n';
        sqlite3_free(err_msg);
        return false;
    }

    if (sqlite3_exec(m_db, create_volumes_table, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << '\n';
        sqlite3_free(err_msg);
        return false;
    }

    if (sqlite3_exec(m_db, create_images_table, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << '\n';
        sqlite3_free(err_msg);
        return false;
    }

    if (sqlite3_exec(m_db, create_networks_table, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << '\n';
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

void DatabaseManager::manual_cleanup(){
    if(m_db){
        sqlite3_close(m_db);
    }
}

bool DatabaseManager::add_container(const ContainerObject& container) {
    const char* sql{ "INSERT INTO containers (id, name, image, pid, net_pid, status, hostname, filesystem_path, pty_shell, vfs, no_remove, vfs_path) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);" };
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << '\n';
        return false;
    }

    sqlite3_bind_text(stmt, 1, container.id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, container.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, container.image.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, container.pid);
    sqlite3_bind_int(stmt, 5, container.net_pid);
    sqlite3_bind_text(stmt, 6, container.status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, container.hostname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, container.filesystem_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, container.pty_shell.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 10, container.vfs ? 1 : 0);
    sqlite3_bind_int(stmt, 11, container.no_remove ? 1 : 0);
    sqlite3_bind_text(stmt, 12, container.vfs_path.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

ContainerObject DatabaseManager::get_container(const std::string& container_id) {
    const char* sql{ "SELECT id, name, image, pid, net_pid, status, created_at, hostname, filesystem_path, pty_shell, vfs, no_remove, vfs_path FROM containers WHERE id = ?;" };
    sqlite3_stmt* stmt{};
    ContainerObject container{};

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            container.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            container.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            container.image = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            container.pid = sqlite3_column_int(stmt, 3);
            container.net_pid = sqlite3_column_int(stmt, 4);
            container.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            container.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            container.hostname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            container.filesystem_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            container.pty_shell = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            container.vfs = sqlite3_column_int(stmt, 10);
            container.no_remove = sqlite3_column_int(stmt, 11);
            const unsigned char* path = sqlite3_column_text(stmt, 12);
            container.vfs_path = path ? reinterpret_cast<const char*>(path) : "";
        }
    }
    sqlite3_finalize(stmt);
    return container;
}

bool DatabaseManager::update_container_status(const std::string& container_id, const std::string& status) {
    const char* sql = "UPDATE containers SET status = ? WHERE id = ?;";
    sqlite3_stmt* stmt{nullptr};

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << '\n';
        return false;
    }

    if (sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, container_id.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        std::cerr << "Failed to bind parameters: " << sqlite3_errmsg(m_db) << '\n';
        sqlite3_finalize(stmt);
        return false;
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(m_db) << '\n';
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool DatabaseManager::update_container_pid(const std::string& container_id, pid_t pid) {
    const char* sql = "UPDATE containers SET pid = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, pid);
    sqlite3_bind_text(stmt, 2, container_id.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::remove_container(const std::string& container_name) {
    const char* sql = "DELETE FROM containers WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, container_name.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::container_exists(const std::string& container_id){
    const char* sql{ "SELECT EXISTS(SELECT 1 FROM containers WHERE id = ?);" };
    sqlite3_stmt* stmt{};
    bool exists{ false };

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) != 0;
    }

    sqlite3_finalize(stmt);
    return exists;
}

std::vector<ContainerObject> DatabaseManager::list_all_containers() {
    const char* sql = "SELECT id, name, image, pid, net_pid, status, created_at, hostname, filesystem_path, pty_shell, vfs, no_remove, vfs_path FROM containers;";
    sqlite3_stmt* stmt;
    std::vector<ContainerObject> containers;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ContainerObject c;
            c.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            c.image = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            c.pid = sqlite3_column_int(stmt, 3);
            c.net_pid = sqlite3_column_int(stmt, 4); // Don't forget this new one too
            c.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            c.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            c.hostname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            c.filesystem_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            c.pty_shell = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            c.vfs = sqlite3_column_int(stmt, 10);
            c.no_remove = sqlite3_column_int(stmt, 11);
            const unsigned char* path = sqlite3_column_text(stmt, 12);
            c.vfs_path = path ? reinterpret_cast<const char*>(path) : "";

            containers.push_back(c);
        }
    }
    sqlite3_finalize(stmt);
    return containers;
}

std::vector<ContainerObject> DatabaseManager::list_running_containers() {
    const char* sql = "SELECT id, name, image, pid, net_pid, status, created_at, hostname, filesystem_path, pty_shell, vfs, no_remove, vfs_path FROM containers WHERE status = 'running';";
    sqlite3_stmt* stmt;
    std::vector<ContainerObject> containers;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ContainerObject c;
            c.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            c.image = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            c.pid = sqlite3_column_int(stmt, 3);
            c.net_pid = sqlite3_column_int(stmt, 4);
            c.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            c.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            c.hostname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            c.filesystem_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            c.pty_shell = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            c.vfs = sqlite3_column_int(stmt, 10);
            c.no_remove = sqlite3_column_int(stmt, 11);
            const unsigned char* path = sqlite3_column_text(stmt, 12);
            c.vfs_path = path ? reinterpret_cast<const char*>(path) : "";

            containers.push_back(c);
        }
    }
    sqlite3_finalize(stmt);
    return containers;
}

std::vector<ContainerObject> DatabaseManager::list_containers_by_image(const std::string& image_name, const std::string& tag) {
    const char* sql = "SELECT id, name, image, pid, status, created_at, hostname, filesystem_path, pty_shell FROM containers WHERE image = ? AND tag = ?;";
    sqlite3_stmt* stmt;
    std::vector<ContainerObject> containers;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, image_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, tag.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ContainerObject c;
            c.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            c.image = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            c.pid = sqlite3_column_int(stmt, 3);
            c.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            c.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            c.hostname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            c.filesystem_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            c.pty_shell = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            containers.push_back(c);
        }
    }
    sqlite3_finalize(stmt);
    return containers;
}

bool DatabaseManager::add_volume(const VolumeObject& volume) {
    const char* sql{ "INSERT OR IGNORE INTO volumes (container_id, host_path, container_path) VALUES (?, ?, ?);" };
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, volume.container_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, volume.host_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, volume.container_path.c_str(), -1, SQLITE_TRANSIENT);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

std::vector<VolumeObject> DatabaseManager::get_container_volumes(const std::string& container_id) {
    const char* sql = "SELECT id, container_id, host_path, container_path FROM volumes WHERE container_id = ?;";
    sqlite3_stmt* stmt;
    std::vector<VolumeObject> volumes;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            VolumeObject v;
            v.id = sqlite3_column_int(stmt, 0);
            v.container_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            v.host_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            v.container_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            volumes.push_back(v);
        }
    }
    sqlite3_finalize(stmt);
    return volumes;
}

std::vector<VolumeObject> DatabaseManager::list_all_volumes() {
    const char* sql = "SELECT id, container_id, host_path, container_path FROM volumes;";
    sqlite3_stmt* stmt;
    std::vector<VolumeObject> volumes;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            VolumeObject v;
            v.id = sqlite3_column_int(stmt, 0);
            v.container_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            v.host_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            v.container_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            volumes.push_back(v);
        }
    }
    sqlite3_finalize(stmt);
    return volumes;
}

bool DatabaseManager::volume_exists(const int& volume_id) {
    const char* sql = "SELECT 1 FROM volumes WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt;
    bool exists = false;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare volume_exists statement: " << sqlite3_errmsg(m_db) << '\n';
        return false;
    }

    sqlite3_bind_int(stmt, 1, volume_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = true;
    }

    sqlite3_finalize(stmt);
    return exists;
}

bool DatabaseManager::remove_volume(const int& volume_id) {
    const char* sql = "DELETE FROM volumes WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, volume_id);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::update_container_name_in_volumes(const int& volume_id, const std::string& container_name) {
    const char* sql = "UPDATE volumes SET container_name = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, container_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, volume_id);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

void DatabaseManager::remove_volumes_by_id(const std::string& container_id){
    const char* sql{ "DELETE FROM volumes WHERE container_id = ?;" };
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare delete volumes statement: " << sqlite3_errmsg(m_db) << '\n';
        return;
    }

    sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to delete volumes for container " << container_id
                  << ": " << sqlite3_errmsg(m_db) << '\n';
    }

    sqlite3_finalize(stmt);
}

bool DatabaseManager::add_image(const std::string& image_name, const std::string& image_path, long long image_size) {
    std::string name, tag;
    size_t pos = image_name.find(':');
    if (pos != std::string::npos) {
        name = image_name.substr(0, pos);
        tag = image_name.substr(pos + 1);
    } else {
        name = image_name;
        tag = "latest";
    }

    const char* sql = "INSERT INTO images (name, tag, path, size) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, tag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, image_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, image_size);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::remove_image(const std::string& image_name, const std::string& tag) {
    const char* sql = "DELETE FROM images WHERE name = ? AND tag = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, image_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, tag.c_str(), -1, SQLITE_TRANSIENT);

    int step_result = sqlite3_step(stmt);
    bool success = (step_result == SQLITE_DONE);

    if (success) {
        if (sqlite3_changes(m_db) == 0) {
            success = false;
        }
    }

    sqlite3_finalize(stmt);
    return success;
}

std::vector<ImageObject> DatabaseManager::list_all_images() {
    const char* sql = "SELECT id, name, tag, path, size, created_at FROM images;";
    sqlite3_stmt* stmt;
    std::vector<ImageObject> images;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ImageObject img;
            img.id = sqlite3_column_int(stmt, 0);
            img.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            img.tag = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            img.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            img.size = sqlite3_column_int64(stmt, 4);
            img.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            images.push_back(img);
        }
    }
    sqlite3_finalize(stmt);
    return images;
}

bool DatabaseManager::network_exists(const int& network_id) {
    const char* sql = "SELECT 1 FROM networks WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt;
    bool exists = false;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare network_exists statement: " << sqlite3_errmsg(m_db) << '\n';
        return false;
    }

    sqlite3_bind_int(stmt, 1, network_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = true;
    }

    sqlite3_finalize(stmt);
    return exists;
}

bool DatabaseManager::remove_network(const int& network_id){
    const char* sql = "DELETE FROM networks WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare delete network statement: " << sqlite3_errmsg(m_db) << '\n';
        return false;
    }

    sqlite3_bind_int(stmt, 1, network_id);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::create_ports(const std::string& container_id, const std::vector<std::pair<int, int>>& ports) {
    const char* query{ "INSERT OR IGNORE INTO networks (container_id, host_port, container_port) VALUES (?, ?, ?);" };
    sqlite3_stmt* stmt{ nullptr };
    char* err_msg{ nullptr };

    if (sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }

    if (sqlite3_prepare_v2(m_db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& pr : ports) {
        sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, pr.first);
        sqlite3_bind_int(stmt, 3, pr.second);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);

    if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}

bool DatabaseManager::add_ports(const NetworkObject& network){
    const char* sql{ "INSERT OR IGNORE INTO networks (container_id, host_port, container_port) VALUES (?, ?, ?);" };
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << '\n';
        return false;
    }

    sqlite3_bind_text(stmt, 1, network.container_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, network.host_port);
    sqlite3_bind_int(stmt, 3, network.container_port);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

std::vector<NetworkObject> DatabaseManager::get_all_networks(){
    const char* sql = "SELECT id, container_id, host_port, container_port FROM networks;";
    sqlite3_stmt* stmt;
    std::vector<NetworkObject> networks{};

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            NetworkObject net_obj{};
            net_obj.id = sqlite3_column_int(stmt, 0);
            net_obj.container_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            net_obj.host_port = sqlite3_column_int(stmt, 2);
            net_obj.container_port = sqlite3_column_int(stmt, 3);
            networks.emplace_back(net_obj);
        }
    }
    sqlite3_finalize(stmt);
    return networks;
}

std::vector<std::pair<int,int>> DatabaseManager::get_linked_ports(const std::string& container_id) {
    std::vector<std::pair<int,int>> forward_ports{};

    const char* sql{ "SELECT host_port, container_port FROM networks WHERE container_id = ?;" };

    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return forward_ports;
    }

    sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int h_port{ sqlite3_column_int(stmt, 0) };
        int c_port{ sqlite3_column_int(stmt, 1) };

        forward_ports.emplace_back(std::make_pair(h_port, c_port));
    }

    sqlite3_finalize(stmt);

    return forward_ports;
}

void DatabaseManager::remove_networks_by_id(const std::string& container_id){
    const char* sql{ "DELETE FROM networks WHERE container_id = ?;" };
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare delete volumes statement: " << sqlite3_errmsg(m_db) << '\n';
        return;
    }

    sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to delete volumes for container " << container_id
                  << ": " << sqlite3_errmsg(m_db) << '\n';
    }

    sqlite3_finalize(stmt);
}
