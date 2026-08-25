#include "include/SyncManager.h"
#include "include/AuthManager.h"
#include "include/Backend.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDebug>

namespace Quiver {

SyncManager& SyncManager::get_instance() {
    static SyncManager instance;
    return instance;
}

SyncManager::SyncManager() 
    : QObject(nullptr), settings_("Quiver", "QuiverGUI") 
{
    machine_uuid_ = settings_.value("machine_uuid", "").toString();
    
    // Setup debounce timer
    debounce_timer_.setSingleShot(true);
    debounce_timer_.setInterval(2000); // 2 seconds debounce
    connect(&debounce_timer_, &QTimer::timeout, this, &SyncManager::perform_sync);

    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, &SyncManager::on_database_changed);
}

void SyncManager::set_machine_uuid(const QString& uuid) {
    machine_uuid_ = uuid;
    settings_.setValue("machine_uuid", uuid);
    
    QStringList known = settings_.value("known_uuids").toStringList();
    if (!known.contains(uuid)) {
        known.append(uuid);
        settings_.setValue("known_uuids", known);
    }
    
    settings_.sync();
    
    if (is_sync_enabled()) {
        trigger_sync();
    }
}

QString SyncManager::get_machine_uuid() const {
    return machine_uuid_;
}

QString SyncManager::get_known_uuids() const {
    QStringList known = settings_.value("known_uuids").toStringList();
    QString current = settings_.value("machine_uuid").toString();
    if (!current.isEmpty() && !known.contains(current)) {
        known.append(current);
    }
    return known.join(",");
}

bool SyncManager::is_sync_enabled() const {
    // Sync is disabled if guest mode, or no token, or no machine UUID.
    return !AuthManager::get_instance().is_guest() && 
           !AuthManager::get_instance().get_token().isEmpty() &&
           !machine_uuid_.isEmpty();
}

void SyncManager::trigger_sync() {
    if (!is_sync_enabled()) return;
    
    // Debounce to prevent spamming if many state changes happen at once
    debounce_timer_.start();
}

void SyncManager::start_database_watcher(const QString& db_path) {
    if (!watcher_.files().contains(db_path)) {
        watcher_.addPath(db_path);
    }
}

void SyncManager::on_database_changed(const QString& path) {
    Q_UNUSED(path);
    if (!is_sync_enabled()) return;
    
    // Trigger a sync when Quiver's DB changes (e.g., container crash)
    trigger_sync();
}

void SyncManager::perform_sync() {
    if (!is_sync_enabled()) return;

    // We do NOT modify how GUI fetches local lists! 
    // We just run this parallel background process to sync state to the cloud.
    
    // Build JSON payload
    QJsonArray containersArray;
    for (const auto& c : Backend::get_instance().get_containers()) {
        QJsonObject obj;
        obj["container_id"] = c.id;
        obj["name"] = c.name;
        obj["image"] = c.image;
        obj["status"] = c.status;
        obj["ports"] = c.ports.join(", ");
        containersArray.append(obj);
    }
    
    QJsonObject payload;
    payload["containers"] = containersArray;
    
    QNetworkRequest request(QUrl("http://localhost:8080/api/sync/containers"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + AuthManager::get_instance().get_token().toUtf8());
    request.setRawHeader("X-Machine-UUID", machine_uuid_.toUtf8());
    
    QNetworkReply* reply = network_manager_.post(request, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, [reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            // Fails silently as per constraints, no intrusive UI pop-ups!
            qDebug() << "[SyncManager] Sync failed silently:" << reply->errorString();
        } else {
            qDebug() << "[SyncManager] Sync successful.";
        }
        reply->deleteLater();
    });
}

}
