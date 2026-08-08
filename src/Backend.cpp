#include "include/Backend.h"
#include <QProcess>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>

#include <QRegularExpression>

namespace Quiver {

static QString resolve_cli_path() {
    QDir dir(QCoreApplication::applicationDirPath());
    while (dir.dirName() != "QuiverGUI" && !dir.isRoot()) {
        dir.cdUp();
    }
    return QDir::cleanPath(dir.absoluteFilePath("Quiver/Quiver/build/release/quiver"));
}

struct Backend::BackendImpl {
    QNetworkAccessManager network_; 
    std::vector<Container> containers_ {};

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

auto Backend::get_containers() const -> std::vector<Container> {
    std::vector<Container> result;
    QString cli_path = resolve_cli_path();
    if (!QFile::exists(cli_path)) return result;

    QProcess process;
    process.start(cli_path, QStringList() << "ps" << "-a");
    if (!process.waitForFinished(2000)) {
        process.kill();
        process.waitForFinished(500);
        return result;
    }

    QString output = process.readAllStandardOutput();
    qDebug() << "quiver ps output:" << output;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i];
        QStringList parts = line.split(QRegularExpression("\\s{2,}"), Qt::SkipEmptyParts);
        qDebug() << "Parsed parts for line" << i << ":" << parts;
        if (parts.size() >= 4) {
            Container c;
            c.id = parts[0];
            c.image = parts[1];
            c.name = parts[2];
            c.status = parts[3];
            result.push_back(c);
        }
    }
    return result;
}
auto Backend::add_container(const Container& container) -> void { 
    // We do not modify the hardcoded vector anymore, the CLI is the source of truth
    
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QProcess* process = new QProcess();
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
                         process, &QObject::deleteLater);
                         
        QStringList args;
        args << "run";
        if (container.filesystem.toUpper() == "VFS") {
            args << "--vfs";
        }
        args << "--name" << container.name;
        
        for (const auto& dev : container.devices) {
            // Devices are formatted like "/dev/video0 (Camera)"
            QString path = dev.split(" ").first();
            if (!path.isEmpty()) args << "--device" << path;
        }
        
        for (const auto& vol : container.volumes) {
            // Bind mount exactly as is, GUI selects host folder
            args << "-v" << vol + ":" + vol;
        }
        
        for (const auto& port : container.ports) {
            // Port comes in as "8080:80"
            args << "-p" << port;
        }
        
        // Interactive and Detach flags, and Image positional argument
        args << "-i" << "-d" << container.image;
        
        qDebug() << "Executing Quiver CLI:" << cli_path << args;
        process->start(cli_path, args);
    } else {
        qDebug() << "Quiver CLI not found at" << cli_path << ". Cannot start container.";
    }
}
auto Backend::delete_container(const QStringList& container_ids) -> void {
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QProcess* process = new QProcess();
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         process, &QObject::deleteLater);
        QObject::connect(process, &QProcess::readyReadStandardError, [process]() {
            qDebug() << "quiver rm stderr:" << process->readAllStandardError();
        });
        QObject::connect(process, &QProcess::readyReadStandardOutput, [process]() {
            qDebug() << "quiver rm stdout:" << process->readAllStandardOutput();
        });
        qDebug() << "Executing Quiver CLI for delete:" << cli_path << "rm" << container_ids;
        process->start(cli_path, QStringList() << "rm" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::pause_container(const QStringList& container_ids) -> void {
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QProcess* process = new QProcess();
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         process, &QObject::deleteLater);
        QObject::connect(process, &QProcess::readyReadStandardError, [process]() {
            qDebug() << "quiver pause stderr:" << process->readAllStandardError();
        });
        QObject::connect(process, &QProcess::readyReadStandardOutput, [process]() {
            qDebug() << "quiver pause stdout:" << process->readAllStandardOutput();
        });
        qDebug() << "Executing Quiver CLI for pause:" << cli_path << "pause" << container_ids;
        process->start(cli_path, QStringList() << "pause" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::unpause_container(const QStringList& container_ids) -> void {
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QProcess* process = new QProcess();
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         process, &QObject::deleteLater);
        QObject::connect(process, &QProcess::readyReadStandardError, [process]() {
            qDebug() << "quiver unpause stderr:" << process->readAllStandardError();
        });
        QObject::connect(process, &QProcess::readyReadStandardOutput, [process]() {
            qDebug() << "quiver unpause stdout:" << process->readAllStandardOutput();
        });
        qDebug() << "Executing Quiver CLI for unpause:" << cli_path << "unpause" << container_ids;
        process->start(cli_path, QStringList() << "unpause" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
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
