#include "include/AuthManager.h"
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
namespace Quiver {

struct AuthManager::Impl {
    QNetworkAccessManager network_;
    QString api_base_url_ { "http://localhost:8080/api" }; 
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

QString AuthManager::get_username() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("username", "Guest").toString();
}

QString AuthManager::get_full_name() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("full_name", "Local User").toString();
}

QString AuthManager::get_token() const {
    QSettings settings("QuiverApp", "Quiver");
    return settings.value("jwt_token").toString();
}

void AuthManager::logout() {
    QSettings settings("QuiverApp", "Quiver");
    settings.remove("jwt_token");
    settings.remove("username");
    settings.remove("full_name");
    emit logged_out();
}



void AuthManager::login(const QString& identity, const QString& password) {
    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + "/auth/signin"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["identity"] = identity;
    body["password"] = password;

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

    emit signup_success();
});
}

/
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

void AuthManager::update_profile(const QString& full_name, const QString& username) {
    QNetworkRequest request = make_auth_request("/profile/update");

    
    QStringList parts = full_name.split(' ');
    QJsonObject body;
    body["first_name"] = parts.value(0);
    body["last_name"]  = parts.mid(1).join(' ');
    body["username"]   = username.startsWith('@') ? username.mid(1) : username;

    QNetworkReply* reply = pimpl_->network_.sendCustomRequest(request, "PATCH",
                               QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, full_name, username]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return; 
        QSettings settings("QuiverApp", "Quiver");
        settings.setValue("full_name", full_name);
        QString formatted = username.startsWith('@') ? username : "@" + username;
        settings.setValue("username", formatted);
        emit profile_updated();
    });
}


void AuthManager::fetch_profile() {
    QNetworkRequest request = make_auth_request("/profile/me");
    QNetworkReply* reply = pimpl_->network_.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray raw = reply->readAll();
        
      
        qDebug() << "Profile fetch:" << raw; 

        if (reply->error() != QNetworkReply::NoError) return;

        QJsonObject user = QJsonDocument::fromJson(raw).object()["user"].toObject();

        QSettings settings("QuiverApp", "Quiver");
        settings.setValue("full_name",   user["first_name"].toString() + " " + user["last_name"].toString());
        settings.setValue("username",    "@" + user["username"].toString());
        settings.setValue("avatar_url",  user["avatar_url"].toString());

        emit profile_updated();
        download_and_cache_avatar();
    });
}



void AuthManager::upload_avatar(const QString& file_path) {

    QImage img(file_path);
    if (img.isNull()) {
        qDebug() << "upload_avatar: failed to load image from" << file_path;
        return;
    }

  
    if (img.width() > 512 || img.height() > 512)
        img = img.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  
    QByteArray compressed_data;
    QBuffer buffer(&compressed_data);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "JPEG", 75);
    buffer.close();


    QString new_hash = QString(QCryptographicHash::hash(compressed_data, QCryptographicHash::Md5).toHex());
    QSettings settings("QuiverApp", "Quiver");
    QString old_hash = settings.value("avatar_hash").toString();

    if (new_hash == old_hash) {
        qDebug() << "upload_avatar: image unchanged, skipping upload";
        return;
    }


    QNetworkRequest request(QUrl(pimpl_->api_base_url_ + "/profile/avatar"));
    request.setRawHeader("Authorization", ("Bearer " + get_token()).toUtf8());

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"avatar\"; filename=\"avatar.jpg\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
    filePart.setBody(compressed_data); 
    multiPart->append(filePart);

    QNetworkReply* reply = pimpl_->network_.post(request, multiPart);
    multiPart->setParent(reply);

   connect(reply, &QNetworkReply::finished, this, [this, reply, new_hash]() {
        
        reply->deleteLater();

        QByteArray raw = reply->readAll();
        qDebug() << "Avatar upload response:" << raw;

        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Avatar upload failed:" << reply->errorString();
            return;
        }

        QJsonObject json = QJsonDocument::fromJson(raw).object();
        QString url = json["avatar_url"].toString();

        QSettings settings("QuiverApp", "Quiver");
        settings.setValue("avatar_url", url);
        settings.setValue("avatar_hash", new_hash);  
        download_and_cache_avatar(); 
        emit profile_updated();
    });
}

QString AuthManager::get_cached_avatar_path() const {
    QSettings settings("QuiverApp", "Quiver");
  
    QString cached = settings.value("avatar_cache_path").toString();
    if (!cached.isEmpty() && QFile::exists(cached)) return cached;
    return ":/assets/icons/profile.svg"; 
}

void AuthManager::download_and_cache_avatar() {
    QSettings settings("QuiverApp", "Quiver");
    QString url = settings.value("avatar_url").toString();

    if (url.isEmpty() || url.startsWith(":/")) return;

    QNetworkRequest request{QUrl(url)};
    QNetworkReply* reply = pimpl_->network_.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
       
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Avatar download failed:" << reply->errorString();
            return;
        }

        QByteArray data = reply->readAll();

      
        QString cache_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/quiver_avatar.jpg";
        QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));

        QFile f(cache_path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(data);
            f.close();
        }

        QSettings settings("QuiverApp", "Quiver");
        settings.setValue("avatar_cache_path", cache_path);

        emit profile_updated(); 
    });
}
} 