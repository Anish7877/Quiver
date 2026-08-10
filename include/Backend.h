#pragma once
#include "common_header.hpp"
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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
};

struct Image {
    QString id {};
    QString repository {};
    QString tag {};
    QString size {};
    QString created {};
    QString status {};
};

struct Volume {
    QString name {};
    QString driver {};
    QString mount_point {};
    QString size {};
    QString status {};
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

class Backend {
public:
    Backend();
    ~Backend();

    Backend(const Backend&) = delete;
    Backend(Backend&&) = delete;
    auto operator=(const Backend&) -> Backend& = delete;

    static auto get_instance() -> Backend&;

    auto get_containers() const -> std::vector<Container>;
    auto get_container_inspect(const QString& id) const -> QString;
    auto add_container(const Container& container) -> void;
    auto delete_container(const QStringList& container_ids) -> void;
    auto pause_container(const QStringList& container_ids) -> void;
    auto unpause_container(const QStringList& container_ids) -> void;
    auto start_container(const QStringList& container_ids) -> void;
    auto stop_container(const QStringList& container_ids) -> void;
    auto prune_containers() -> void;

    auto get_images() const -> std::vector<Image>;
    auto add_image(const Image& img) -> void;
    auto delete_image(const QString& image_id) -> void;

    auto get_volumes() const -> std::vector<Volume>;
    auto add_volume(const Volume& vol) -> void;
    auto delete_volume(const QString& volume_name) -> void;

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
};

}
