#pragma once
#include "common_header.hpp"
#include <vector>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include <QObject>

namespace Quiver {

struct Container {
    QString id {};
    QString name {};
    QString image {};
    QString status {};
    QString filesystem {};
    QStringList devices {};
    QStringList volumes {};
    QStringList ports {};
    QString created_at {};
    bool prevent_interaction {false};
    QString command {};
    QString options {};
    int cpu_quota {0};
    int cpu_weight {0};
    QString memory_max {};
    QString memory_swap {};
    int pids_limit {0};
    QString cpuset_cpus {};
    QString cpuset_mems {};
    QString io_weight {};
    QString io_max {};
};

struct Image {
    QString id {};
    QString name {};
    QString tag {};
    QString size {};
    QString source {};
};

struct Volume {
    QString container_id {};
    QString source {};
    QString destination {};
    QString type {};
};

struct PortMapping {
    QString id {};
    QString tcp {};
    QString udp {};
};

struct Device {
    QString path {};
    QString type {};
    QString container_name {};
    QString permissions {};
    QString status {};
};

class Backend : public QObject {
    Q_OBJECT
public:
    Backend();
    ~Backend();

    Backend(const Backend&) = delete;
    Backend(Backend&&) = delete;
    auto operator=(const Backend&) -> Backend& = delete;

    static auto get_instance() -> Backend&;

    auto get_containers() const -> std::vector<Container>;
    auto get_container_inspect(const QString& id) const -> QString;
    auto get_container_top(const QString& id) const -> QString;
    auto add_container(const Container& container) -> QProcess*;
    auto delete_container(const QStringList& container_ids) -> void;
    auto pause_container(const QStringList& container_ids) -> void;
    auto unpause_container(const QStringList& container_ids) -> void;
    auto start_container(const QStringList& container_ids) -> void;
    auto stop_container(const QStringList& container_ids) -> void;
    auto restart_container(const QString& id) -> void;
    auto update_container(const Container& container) -> void;
    auto prune_containers() -> void;

    auto get_images() const -> std::vector<Image>;
    auto add_image(const Image& img) -> void;
    auto delete_images(const QStringList& targets) -> void;
    auto pull_image(const QString& name, const QString& tag) -> void;
    auto load_image(const QString& name, const QString& tag, const QString& tar_path) -> void;

    auto get_volumes() const -> std::vector<Volume>;
    auto add_volume(const QString& container_id, const QString& host_path, const QString& container_path, const QString& mode) -> void;
    auto delete_volumes(const QMap<QString, QStringList>& targets) -> void;

    auto get_ports() const -> std::vector<PortMapping>;
    auto add_port(const PortMapping& port) -> void;
    auto delete_port(const QString& port_id) -> void;
    
    auto get_cli_path() const -> QString;
    auto get_last_error() const -> QString;

    auto get_devices() const -> std::vector<Device>;
    auto add_device(const Device& device) -> void;
    auto delete_device(const QString& device_path) -> void;

private:
    struct BackendImpl;
    std::unique_ptr<BackendImpl> pimpl_ {};

signals:
    void cli_error_occurred(const QString& error_msg);
    void cli_action_success(const QString& success_msg);
};

}
