#include "include/AuthManager.h"
#include "include/SyncManager.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QFile>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QImage>
#include <QBuffer>
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDesktopServices>
#include <QRandomGenerator>
#include <QUrl>
#include <QRegularExpression>
namespace Quiver {

struct AuthManager::Impl {
    QNetworkAccessManager network_;
    QString api_base_url_ { "http://localhost:8080/api" }; 
    QTcpServer* auth_server_ { nullptr };
};

AuthManager::AuthManager() : pimpl_{std::make_unique<Impl>()} {}
AuthManager::~AuthManager() = default;

auto AuthManager::get_instance() -> AuthManager& {
    static AuthManager instance;
    return instance;
}

bool AuthManager::is_logged_in() const {
    QSettings settings("QuiverApp", "Quiver");
    return !settings.value("jwt_token").toString().isEmpty();
}

bool AuthManager::is_guest() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("jwt_token").toString() == "guest_mode";
}

QString AuthManager::get_username() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("username", "Guest").toString();
}

QString AuthManager::get_full_name() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("full_name", "Local User").toString();
}

QString AuthManager::get_hub_username() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("hub_username").toString();
}

QString AuthManager::get_hub_token() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("hub_token").toString();
}

QString AuthManager::get_token() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("jwt_token").toString();
}
void AuthManager::start_browser_login() {
    if (!pimpl_->auth_server_) {
        pimpl_->auth_server_ = new QTcpServer(this);
        
        connect(pimpl_->auth_server_, &QTcpServer::newConnection, this, [this]() {
            QTcpSocket* socket = pimpl_->auth_server_->nextPendingConnection();
            
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
                QByteArray request = socket->readAll();
                
                
                QRegularExpression rx("GET /callback\\?token=([^&\\s]+)(?:&machine_uuid=([^\\s&]+))?");
                QRegularExpressionMatch match = rx.match(QString(request));

                if (match.hasMatch()) {
                    QString token = match.captured(1);
                    QString uuid = match.captured(2);
                    
                    if (!uuid.isEmpty()) {
                        Quiver::SyncManager::get_instance().set_machine_uuid(uuid);
                    }
                    
                    
                    QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                                          "<html><body style='background:#09090b; color:#fafafa; font-family:sans-serif; display:flex; align-items:center; justify-content:center; height:100vh; margin:0;'>"
                                          "<div style='text-align:center;'><h2>Authentication Successful</h2><p style='color:#a1a1aa;'>You can safely close this tab and return to Quiver.</p></div>"
                                          "<script>setTimeout(()=>window.close(), 2000);</script></body></html>";
                    socket->write(response);
                    socket->flush();
                    socket->disconnectFromHost();

                    
                    QSettings settings("QuiverApp", "Quiver");
                    settings.setValue("jwt_token", token);
                    
                    pimpl_->auth_server_->close(); 
                    
                    fetch_profile(); 
                    emit login_success(); 
                }
            });
        });
    }

    if (!pimpl_->auth_server_->isListening()) {
        pimpl_->auth_server_->listen(QHostAddress::LocalHost, 54321);
    }

 
    QString existing_uuids = Quiver::SyncManager::get_instance().get_known_uuids();
    QDesktopServices::openUrl(QUrl("http://localhost:8080/web/auth?port=54321&uuid=" + existing_uuids));
}


void AuthManager::logout() {
    QSettings settings("QuiverApp", "Quiver");
    settings.remove("jwt_token");
    settings.remove("username");
    settings.remove("full_name");
    
   
    settings.remove("avatar_url");
    settings.remove("avatar_cache_path");
    settings.remove("avatar_hash");
    settings.remove("hub_username");
    settings.remove("hub_token");
    
    emit logged_out();
}

void AuthManager::guest_login() {
    QSettings settings("QuiverApp", "Quiver");
    settings.setValue("jwt_token", "guest_mode");
    
    // Clear any previous guest's offline configs
    settings.remove("offline_configs");
    
    QString chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString randomString;
    for(int i=0; i<6; ++i) {
        randomString.append(chars.at(QRandomGenerator::global()->bounded(chars.length())));
    }
    settings.setValue("username", "@Guest_" + randomString);
    settings.setValue("full_name", "Guest User");
    emit login_success();
}

void AuthManager::login(const QString& identity, const QString& password) {
    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + "/auth/signin"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["identity"] = identity;
    body["password"] = password;
    body["is_gui"] = true; // Request a MachineUUID

    QString existing_uuid = Quiver::SyncManager::get_instance().get_machine_uuid();
    if (!existing_uuid.isEmpty()) {
        body["machine_uuid"] = existing_uuid;
    }

    QNetworkReply* reply = pimpl_->network_.post(request, QJsonDocument(body).toJson());

   
connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    QByteArray raw = reply->readAll();
    qDebug() << "Signup response code:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "Signup response body:" << raw;

    QJsonObject json = QJsonDocument::fromJson(raw).object();

    if (reply->error() != QNetworkReply::NoError) {
        emit signup_failed(json["error"].toString());
        return;
    }

    QString token = json["token"].toString();
    QJsonObject user = json["user"].toObject();

    QSettings settings("QuiverApp", "Quiver");
    settings.setValue("jwt_token", token);
    settings.setValue("username", "@" + user["username"].toString());
    settings.setValue("full_name", user["first_name"].toString() + " " + user["last_name"].toString());
    settings.setValue("avatar_url", user["avatar_url"].toString());

    if (json.contains("machine_uuid")) {
        Quiver::SyncManager::get_instance().set_machine_uuid(json["machine_uuid"].toString());
    }

    emit signup_success();
});
}
void AuthManager::signUp(const QString& first_name, const QString& last_name,
                          const QString& username, const QString& email, const QString& password) {
    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + "/auth/signup"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["first_name"] = first_name;
    body["last_name"]  = last_name;
    body["username"]   = username;
    body["email"]      = email;
    body["password"]   = password;
    body["is_gui"]     = true; // Request a MachineUUID

    qDebug() << "Sending signup request to:" << pimpl_->api_base_url_ + "/auth/signup";
    qDebug() << "Body:" << QJsonDocument(body).toJson();

    QNetworkReply* reply = pimpl_->network_.post(request, QJsonDocument(body).toJson());

connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray raw = reply->readAll();
        qDebug() << "Signup status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qDebug() << "Signup body:" << raw;

        QJsonObject json = QJsonDocument::fromJson(raw).object();

        if (reply->error() != QNetworkReply::NoError) {
            emit signup_failed(json["error"].toString());
            return;
        }

        QString token = json["token"].toString();
        QJsonObject user = json["user"].toObject();

        QSettings settings("QuiverApp", "Quiver");
        settings.setValue("jwt_token", token);
        settings.setValue("username", "@" + user["username"].toString());
        settings.setValue("full_name", user["first_name"].toString() + " " + user["last_name"].toString());
        settings.setValue("avatar_url", user["avatar_url"].toString());

        if (json.contains("machine_uuid")) {
            Quiver::SyncManager::get_instance().set_machine_uuid(json["machine_uuid"].toString());
        }

        emit signup_success();
    });
}

QString AuthManager::get_avatar_path() const {
    QSettings settings("QuiverApp", "Quiver");
    QString url = settings.value("avatar_url").toString();
    if (!url.isEmpty()) return url;
    return settings.value("avatar_path", ":/assets/icons/profile.svg").toString();
}

void AuthManager::set_avatar_path(const QString& path) {
    QSettings settings("QuiverApp", "Quiver");
    settings.setValue("avatar_path", path);
    emit profile_updated(); 
}


QNetworkRequest AuthManager::make_auth_request(const QString& endpoint) const {
    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + get_token()).toUtf8());
    return request;
}

void AuthManager::update_hub_credentials(const QString& hub_username, const QString& hub_token) {
    if (is_guest()) return;

    QJsonObject json;
    json["hub_username"] = hub_username;
    json["hub_token"] = hub_token;

    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + "/profile/hub-credentials"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + get_token()).toUtf8());

    QNetworkReply* reply = pimpl_->network_.post(request, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, hub_username, hub_token]() {
        if (reply->error() == QNetworkReply::NoError) {
            QSettings settings("QuiverApp", "Quiver");
            settings.setValue("hub_username", hub_username);
            settings.setValue("hub_token", hub_token); // Cache locally for CLI use
            emit profile_updated();
        } else {
            emit login_failed("Failed to update Hub Credentials: " + reply->errorString());
        }
        reply->deleteLater();
    });
}


void AuthManager::update_profile(const QString& full_name, const QString& username) {
    if (is_guest()) return;
    QJsonObject json;
    
    // basic split for full name
    QStringList parts = full_name.split(" ");
    json["first_name"] = parts.isEmpty() ? "" : parts[0];
    json["last_name"] = parts.size() > 1 ? parts.mid(1).join(" ") : "";
    
    QString clean_username = username;
    if (clean_username.startsWith("@")) {
        clean_username = clean_username.mid(1);
    }
    json["username"] = clean_username;

    QNetworkRequest request = make_auth_request("/profile/update");
    QNetworkReply* reply = pimpl_->network_.sendCustomRequest(request, "PATCH", QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, full_name, username]() {
        if (reply->error() == QNetworkReply::NoError) {
            QSettings settings("QuiverApp", "Quiver");
            settings.setValue("full_name", full_name);
            settings.setValue("username", username);
            emit profile_updated();
        }
        reply->deleteLater();
    });
}

void AuthManager::upload_avatar(const QString& file_path) {
    if (is_guest()) return;
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart imagePart;
    imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"avatar\"; filename=\"avatar.jpg\""));

    QFile *file = new QFile(file_path);
    file->open(QIODevice::ReadOnly);
    imagePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(imagePart);

    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + "/profile/avatar"));
    request.setRawHeader("Authorization", ("Bearer " + get_token()).toUtf8());
    QNetworkReply* reply = pimpl_->network_.post(request, multiPart);
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            fetch_profile();
        }
        reply->deleteLater();
    });
}

void AuthManager::fetch_profile() {
    if (is_guest()) return;
    QNetworkRequest request = make_auth_request("/profile/me");
    QNetworkReply* reply = pimpl_->network_.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
            QJsonObject user = json["user"].toObject();
            QSettings settings("QuiverApp", "Quiver");
            settings.setValue("full_name", user["first_name"].toString() + " " + user["last_name"].toString());
            settings.setValue("username", "@" + user["username"].toString());
            settings.setValue("email", user["email"].toString());
            settings.setValue("avatar_url", user["avatar_url"].toString());
            settings.setValue("hub_username", user["hub_username"].toString());
            if (user.contains("hub_token") && !user["hub_token"].toString().isEmpty()) {
                settings.setValue("hub_token", user["hub_token"].toString());
            }
            emit profile_updated();
            download_and_cache_avatar();
        }
        reply->deleteLater();
    });
}

void AuthManager::download_and_cache_avatar() {
    QString url = get_avatar_path();
    if (url.startsWith(":/") || url.isEmpty()) return;

    QNetworkRequest request((QUrl(url)));
    QNetworkReply* reply = pimpl_->network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QString cache_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/avatar.jpg";
            QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
            QFile file(cache_path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
                QSettings settings("QuiverApp", "Quiver");
                settings.setValue("avatar_cache_path", cache_path);
                emit profile_updated();
            }
        }
        reply->deleteLater();
    });
}

QString AuthManager::get_cached_avatar_path() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("avatar_cache_path", get_avatar_path()).toString();
}

void AuthManager::save_config(const QJsonObject& config) {
    if (is_guest()) return;
    QNetworkRequest request = make_auth_request("/configs");
    QNetworkReply* reply = pimpl_->network_.post(request, QJsonDocument(config).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            get_configs();
        }
        reply->deleteLater();
    });
}

void AuthManager::get_configs(int retries) {
    if (is_guest()) return;
    QNetworkRequest request = make_auth_request("/configs");
    QNetworkReply* reply = pimpl_->network_.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, retries]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("configs") && obj["configs"].isArray()) {
                    emit configs_loaded(obj["configs"].toArray());
                }
            }
        } else if (reply->error() == QNetworkReply::ConnectionRefusedError && retries > 0) {
            QTimer::singleShot(1000, this, [this, retries]() {
                get_configs(retries - 1);
            });
        }
        reply->deleteLater();
    });
}

void AuthManager::delete_all_configs() {
    if (is_guest()) return;
    QNetworkRequest request = make_auth_request("/configs");
    QNetworkReply* reply = pimpl_->network_.deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            emit configs_loaded(QJsonArray()); // Instantly clear UI
        }
        reply->deleteLater();
    });
}

void AuthManager::rename_machine(const QString& name) {
    if (is_guest() || get_token().isEmpty()) return;
    
    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + "/machine/rename"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + get_token().toUtf8());
    
    QJsonObject payload;
    payload["machine_uuid"] = Quiver::SyncManager::get_instance().get_machine_uuid();
    payload["friendly_name"] = name;
    
    QNetworkReply* reply = pimpl_->network_.sendCustomRequest(request, "PATCH", QJsonDocument(payload).toJson());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            emit machine_renamed();
        } else {
            qDebug() << "Machine rename failed:" << reply->errorString() << reply->readAll();
        }
    });
}

} // namespace Quiver

