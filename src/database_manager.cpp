#include "../include/database_manager.hpp"
#include <iostream>
#include <stdexcept>

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
        "status TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "hostname TEXT,"
        "filesystem_path TEXT,"
        "pty_shell TEXT"
        ");" };

    const char* create_volumes_table{
        "CREATE TABLE IF NOT EXISTS volumes ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "container_name TEXT,"
        "host_path TEXT NOT NULL,"
        "container_path TEXT NOT NULL,"
        "FOREIGN KEY(container_name) REFERENCES containers(name)"
        ");" };

    const char* create_images_table{
        "CREATE TABLE IF NOT EXISTS images ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE,"
        "tag TEXT NOT NULL,"
        "path TEXT NOT NULL,"
        "size INT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");" };

    char* err_msg{ nullptr };
    if (sqlite3_exec(m_db, create_containers_table, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    if (sqlite3_exec(m_db, create_volumes_table, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    if (sqlite3_exec(m_db, create_images_table, 0, 0, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }

    return true;
}

bool DatabaseManager::add_container(const ContainerObject& container) {
    const char* sql{ "INSERT INTO containers (id, name, image, pid, status, hostname, filesystem_path, pty_shell) VALUES (?, ?, ?, ?, ?, ?, ?, ?);" };
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, container.id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, container.name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, container.image.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, container.pid);
    sqlite3_bind_text(stmt, 5, container.status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, container.hostname.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, container.filesystem_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, container.pty_shell.c_str(), -1, SQLITE_STATIC);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

// Retrieves a container from the database by its ID
ContainerObject DatabaseManager::get_container(const std::string& container_id) {
    const char* sql{ "SELECT id, name, image, pid, status, created_at, hostname, filesystem_path, pty_shell FROM containers WHERE id = ?;" };
    sqlite3_stmt* stmt{};
    ContainerObject container{};

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            container.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            container.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            container.image = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            container.pid = sqlite3_column_int(stmt, 3);
            container.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            container.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            container.hostname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            container.filesystem_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            container.pty_shell = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
        }
    }
    sqlite3_finalize(stmt);
    return container;
}

bool DatabaseManager::update_container_status(const std::string& container_name, const std::string& status) {
    const char* sql = "UPDATE containers SET status = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, container_name.c_str(), -1, SQLITE_STATIC);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::update_container_pid(const std::string& container_id, pid_t pid) {
    const char* sql = "UPDATE containers SET pid = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_int(stmt, 1, pid);
    sqlite3_bind_text(stmt, 2, container_id.c_str(), -1, SQLITE_STATIC);

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
    sqlite3_bind_text(stmt, 1, container_name.c_str(), -1, SQLITE_STATIC);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

std::vector<ContainerObject> DatabaseManager::list_all_containers() {
    const char* sql = "SELECT id, name, image, pid, status, created_at, hostname, filesystem_path, pty_shell FROM containers;";
    sqlite3_stmt* stmt;
    std::vector<ContainerObject> containers;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
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

std::vector<ContainerObject> DatabaseManager::list_running_containers() {
    const char* sql = "SELECT id, name, image, pid, status, created_at, hostname, filesystem_path, pty_shell FROM containers WHERE status = 'running';";
    sqlite3_stmt* stmt;
    std::vector<ContainerObject> containers;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
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

std::vector<ContainerObject> DatabaseManager::list_containers_by_image(const std::string& image_name) {
    const char* sql = "SELECT id, name, image, pid, status, created_at, hostname, filesystem_path, pty_shell FROM containers WHERE image = ?;";
    sqlite3_stmt* stmt;
    std::vector<ContainerObject> containers;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, image_name.c_str(), -1, SQLITE_TRANSIENT);
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
    const char* sql = "INSERT INTO volumes (container_name, host_path, container_path) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, volume.container_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, volume.host_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, volume.container_path.c_str(), -1, SQLITE_STATIC);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

std::vector<VolumeObject> DatabaseManager::get_container_volumes(const std::string& container_id) {
    const char* sql = "SELECT id, container_name, host_path, container_path FROM volumes WHERE container_id = ?;";
    sqlite3_stmt* stmt;
    std::vector<VolumeObject> volumes;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, container_id.c_str(), -1, SQLITE_STATIC);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            VolumeObject v;
            v.id = sqlite3_column_int(stmt, 0);
            v.container_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            v.host_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            v.container_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            volumes.push_back(v);
        }
    }
    sqlite3_finalize(stmt);
    return volumes;
}

std::vector<VolumeObject> DatabaseManager::list_all_volumes() {
    const char* sql = "SELECT id, container_name, host_path, container_path FROM volumes;";
    sqlite3_stmt* stmt;
    std::vector<VolumeObject> volumes;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            VolumeObject v;
            v.id = sqlite3_column_int(stmt, 0);
            v.container_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            v.host_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            v.container_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            volumes.push_back(v);
        }
    }
    sqlite3_finalize(stmt);
    return volumes;
}

bool DatabaseManager::remove_volume(int volume_id) {
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
    sqlite3_bind_text(stmt, 1, container_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, volume_id);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
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

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, tag.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, image_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, image_size);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
}

bool DatabaseManager::remove_image(const std::string& image_name) {
    const char* sql = "DELETE FROM images WHERE name = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, image_name.c_str(), -1, SQLITE_STATIC);

    bool result = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return result;
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

int DatabaseManager::exec_callback(void* data, int argc, char** argv, char** azColName) {
    return 0;
}
