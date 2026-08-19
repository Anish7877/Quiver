#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
class QTcpServer;
namespace Quiver {

class AuthManager : public QObject {
    Q_OBJECT
public:
    static auto get_instance() -> AuthManager&;

    bool is_logged_in() const;
    bool is_guest() const;
    QString get_username() const;
    QString get_full_name() const;
    QString get_token() const;
QString get_avatar_path() const;
    void set_avatar_path(const QString& path);
    void update_profile(const QString& full_name, const QString& username);

    void start_browser_login();
    
    void login(const QString& identity, const QString& password);
    void guest_login();
    void logout();
    void signUp(const QString& first_name, const QString& last_name,
            const QString& username, const QString& email, const QString& password);
    void upload_avatar(const QString& file_path);
    void fetch_profile();
    void download_and_cache_avatar();
    QString get_cached_avatar_path() const;

    void save_config(const QJsonObject& config);
    void get_configs();

signals:
    void login_success();
    void login_failed(const QString& error_message);
    void logged_out();
    void profile_updated();
    void signup_success();
    void signup_failed(const QString& error_message);
    void profile_save_failed(const QString& error);
    void configs_loaded(const QJsonArray& configs);

private:
    AuthManager();
 
QNetworkRequest make_auth_request(const QString& endpoint) const;
    ~AuthManager() override;
    
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} 