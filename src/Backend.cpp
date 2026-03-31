#include "include/Backend.h"

namespace Quiver {

struct Backend::BackendImpl {
    QNetworkAccessManager network_; 
    std::vector<Container> containers_ {
        {"7f8a1b", "nginx-proxy",  "nginx:alpine",  "running"},
        {"3c4d5e", "redis-cache",  "redis:6.2",     "stopped"},
        {"9a0b1c", "postgres-db",  "postgres:14",   "running"}
    };

    std::vector<Image> images_ {
        {"a1b2c3", "nginx",      "alpine",  "23.4 MB",  "2 days ago",  "available"},
        {"d4e5f6", "redis",      "6.2",     "113 MB",   "5 days ago",  "available"},
        {"g7h8i9", "postgres",   "14",      "376 MB",   "1 week ago",  "available"},
        {"j0k1l2", "ubuntu",     "22.04",   "77.8 MB",  "2 weeks ago", "unused"},
        {"m3n4o5", "alpine",     "latest",  "7.05 MB",  "3 weeks ago", "unused"}
    };

    std::vector<Volume> volumes_ {
        {"pgdata",        "local",   "/var/lib/docker/volumes/pgdata/_data",   "512 MB",  "mounted"},
        {"redis-data",    "local",   "/var/lib/docker/volumes/redis-data/_data","128 MB", "mounted"},
        {"nginx-config",  "local",   "/var/lib/docker/volumes/nginx-config/_data","2 MB", "unmounted"},
        {"app-logs",      "local",   "/var/lib/docker/volumes/app-logs/_data",  "64 MB",  "unmounted"}
    };

    std::vector<PortMapping> ports_ {
        {"p001", "nginx-proxy",  "80",   "80",   "TCP",  "active"},
        {"p002", "nginx-proxy",  "443",  "443",  "TCP",  "active"},
        {"p003", "redis-cache",  "6379", "6379", "TCP",  "inactive"},
        {"p004", "postgres-db",  "5432", "5432", "TCP",  "active"}
    };

    std::vector<Device> devices_ {
        {"/dev/video0",     "Camera",  "nginx-proxy",  "rwm",  "assigned"},
        {"/dev/dri/card0",  "GPU",     "postgres-db",  "rw",   "assigned"},
        {"/dev/ttyUSB0",    "Serial",  "",             "rw",   "available"},
        {"/dev/snd",        "Audio",   "",             "rw",   "available"}
    };
};

Backend::Backend() : pimpl_{std::make_unique<BackendImpl>()} {}
Backend::~Backend() = default;

auto Backend::get_instance() -> Backend& {
    static Backend instance {};
    return instance;
}

auto Backend::get_containers() const -> std::vector<Container> { return pimpl_->containers_; }
auto Backend::add_container(const Container& container) -> void { pimpl_->containers_.push_back(container); }
auto Backend::delete_container(const QString& container_id) -> void {
    auto& c { pimpl_->containers_ };
    c.erase(std::remove_if(c.begin(), c.end(),
        [&container_id](const Container& x){ return x.id == container_id; }), c.end());
}

auto Backend::get_images() const -> std::vector<Image> { return pimpl_->images_; }
auto Backend::add_image(const Image& img) -> void { pimpl_->images_.push_back(img); }
auto Backend::delete_image(const QString& image_id) -> void {
    auto& v { pimpl_->images_ };
    v.erase(std::remove_if(v.begin(), v.end(),
        [&image_id](const Image& x){ return x.id == image_id; }), v.end());
}

auto Backend::get_volumes() const -> std::vector<Volume> { return pimpl_->volumes_; }
auto Backend::add_volume(const Volume& vol) -> void { pimpl_->volumes_.push_back(vol); }
auto Backend::delete_volume(const QString& volume_name) -> void {
    auto& v { pimpl_->volumes_ };
    v.erase(std::remove_if(v.begin(), v.end(),
        [&volume_name](const Volume& x){ return x.name == volume_name; }), v.end());
}

auto Backend::get_ports() const -> std::vector<PortMapping> { return pimpl_->ports_; }
auto Backend::add_port(const PortMapping& port) -> void { pimpl_->ports_.push_back(port); }
auto Backend::delete_port(const QString& port_id) -> void {
    auto& v { pimpl_->ports_ };
    v.erase(std::remove_if(v.begin(), v.end(),
        [&port_id](const PortMapping& x){ return x.id == port_id; }), v.end());
}

auto Backend::get_devices() const -> std::vector<Device> { return pimpl_->devices_; }
auto Backend::add_device(const Device& device) -> void { pimpl_->devices_.push_back(device); }
auto Backend::delete_device(const QString& device_path) -> void {
    auto& v { pimpl_->devices_ };
    v.erase(std::remove_if(v.begin(), v.end(),
        [&device_path](const Device& x){ return x.path == device_path; }), v.end());
}

}
