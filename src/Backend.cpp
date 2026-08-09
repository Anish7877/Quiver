#include "include/Backend.h"
#include <QProcess>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

#include <QRegularExpression>

namespace Quiver {

static void log_debug(const QString& msg) {
    QFile file("/tmp/quiver_gui_debug.txt");
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ") << msg << "\n";
    }
}

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

    std::vector<PortMapping> ports_ {};

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

auto Backend::get_cli_path() const -> QString {
    return resolve_cli_path();
}

auto Backend::get_containers() const -> std::vector<Container> {
    std::vector<Container> result;
    QString cli_path = resolve_cli_path();
    log_debug("get_containers called, cli_path: " + cli_path);
    if (!QFile::exists(cli_path)) {
        log_debug("cli_path does not exist");
        return result;
    }

    QProcess process;
    process.start(cli_path, QStringList() << "ps" << "-a");
    log_debug("Started quiver ps -a");
    if (!process.waitForFinished(10000)) {
        log_debug("waitForFinished TIMEOUT!");
        process.kill();
        process.waitForFinished(500);
        return result;
    }

    QString output = process.readAllStandardOutput();
    QString err = process.readAllStandardError();
    log_debug("Process finished. Stdout length: " + QString::number(output.length()) + ", Stderr: " + err);
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    log_debug("Lines count: " + QString::number(lines.size()));
    
    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i];
        QStringList parts = line.split(QRegularExpression("\\s{2,}"), Qt::SkipEmptyParts);
        if (parts.size() >= 1) {
            Container c;
            c.id = parts[0];
            c.image = parts.size() > 1 ? parts[1] : "";
            c.name = parts.size() > 2 ? parts[2] : "";
            c.status = parts.size() > 3 ? parts[3] : "";
            result.push_back(c);
        }
    }
    log_debug("Parsed containers count: " + QString::number(result.size()));
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
        
        if (!container.options.isEmpty()) {
            args << container.options.split(" ", Qt::SkipEmptyParts);
        }
        
        // Interactive and Detach flags, and Image positional argument
        if (!container.prevent_interaction) {
            args << "-i";
        }
        args << "-d" << container.image;
        if (!container.command.isEmpty()) {
            args << container.command.split(" ", Qt::SkipEmptyParts);
        }
        
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

auto Backend::start_container(const QStringList& container_ids) -> void {
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QProcess* process = new QProcess();
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         process, &QObject::deleteLater);
        QObject::connect(process, &QProcess::readyReadStandardError, [process]() {
            qDebug() << "quiver start stderr:" << process->readAllStandardError();
        });
        QObject::connect(process, &QProcess::readyReadStandardOutput, [process]() {
            qDebug() << "quiver start stdout:" << process->readAllStandardOutput();
        });
        qDebug() << "Executing Quiver CLI for start:" << cli_path << "start" << container_ids;
        process->start(cli_path, QStringList() << "start" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::stop_container(const QStringList& container_ids) -> void {
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QProcess* process = new QProcess();
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         process, &QObject::deleteLater);
        QObject::connect(process, &QProcess::readyReadStandardError, [process]() {
            qDebug() << "quiver stop stderr:" << process->readAllStandardError();
        });
        QObject::connect(process, &QProcess::readyReadStandardOutput, [process]() {
            qDebug() << "quiver stop stdout:" << process->readAllStandardOutput();
        });
        qDebug() << "Executing Quiver CLI for stop:" << cli_path << "stop" << container_ids;
        process->start(cli_path, QStringList() << "stop" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::prune_containers() -> void {
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QProcess* process = new QProcess();
        QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         process, &QObject::deleteLater);
        QObject::connect(process, &QProcess::readyReadStandardError, [process]() {
            qDebug() << "quiver prune stderr:" << process->readAllStandardError();
        });
        QObject::connect(process, &QProcess::readyReadStandardOutput, [process]() {
            qDebug() << "quiver prune stdout:" << process->readAllStandardOutput();
        });
        qDebug() << "Executing Quiver CLI for prune:" << cli_path << "prune";
        process->start(cli_path, QStringList() << "prune");
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

auto Backend::get_ports() const -> std::vector<PortMapping> {
    std::vector<PortMapping> result;
    QString cli_path = resolve_cli_path();
    log_debug("get_ports called, cli_path: " + cli_path);
    if (!QFile::exists(cli_path)) {
        log_debug("cli_path does not exist for ports");
        return result;
    }

    QProcess process;
    process.start(cli_path, QStringList() << "ports");
    log_debug("Started quiver ports");
    if (!process.waitForFinished(10000)) {
        log_debug("waitForFinished TIMEOUT for quiver ports!");
        process.kill();
        process.waitForFinished(500);
        return result;
    }

    QString output = process.readAllStandardOutput();
    QString err = process.readAllStandardError();
    log_debug("Ports process finished. Stdout length: " + QString::number(output.length()) + ", Stderr: " + err);
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    log_debug("Ports lines count: " + QString::number(lines.size()));
    
    QString current_id = "";
    
    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i];
        
        // Output format from CLI: "{:<70} {:<45} {}"
        QString id_part = line.mid(0, 70).trimmed();
        if (!id_part.isEmpty()) {
            current_id = id_part;
        }
        
        QString tcp_part = line.mid(71, 45).trimmed();
        QString udp_part = line.mid(117).trimmed();
        
        if (tcp_part == "-") tcp_part = "";
        if (udp_part == "-") udp_part = "";
        
        PortMapping pm;
        pm.id = current_id;
        pm.tcp = tcp_part;
        pm.udp = udp_part;
        result.push_back(pm);
    }
    log_debug("Parsed ports count: " + QString::number(result.size()));
    return result;
}
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
