#pragma once
#include <QObject>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QNetworkAccessManager>
#include <QString>
#include <QTimer>

namespace Quiver {

class SyncManager : public QObject {
    Q_OBJECT
public:
    static auto get_instance() -> SyncManager&;

    // Call this after a successful GUI login to store the token and activate syncing.
    auto set_machine_uuid(const QString& uuid) -> void;
    auto get_machine_uuid() const -> QString;
    auto get_known_uuids() const -> QString;

    // Checks if the user is authenticated with the global backend.
    auto is_sync_enabled() const -> bool;

    // Manually trigger a sync (e.g. after starting/stopping a container via GUI).
    // If sync is disabled, this returns immediately doing nothing.
    auto trigger_sync() -> void;

    // Start watching the local RocksDB/SQLite file for internal container state changes.
    auto start_database_watcher(const QString& db_path) -> void;

private:
    SyncManager();
    ~SyncManager() = default;
    SyncManager(const SyncManager&) = delete;
    auto operator=(const SyncManager&) -> SyncManager& = delete;

    auto perform_sync() -> void;

    QSettings settings_;
    QFileSystemWatcher watcher_;
    QNetworkAccessManager network_manager_;
    QTimer debounce_timer_;
    QString machine_uuid_;

private slots:
    void on_database_changed(const QString& path);
};

}
