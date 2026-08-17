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

    std::vector<Image> images_ {};

    std::vector<Volume> volumes_ {};

    std::vector<PortMapping> ports_ {};

    std::vector<Device> devices_ {
        {"/dev/video0",     "Camera",  "nginx-proxy",  "rwm",  "assigned"},
        {"/dev/dri/card0",  "GPU",     "postgres-db",  "rw",   "assigned"},
        {"/dev/ttyUSB0",    "Serial",  "",             "rw",   "available"},
        {"/dev/snd",        "Audio",   "",             "rw",   "available"}
    };
    QString last_error_ {};
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

auto Backend::get_last_error() const -> QString {
    return pimpl_->last_error_;
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

    pimpl_->last_error_ = "";
    QString output = process.readAllStandardOutput();
    QString err = process.readAllStandardError();
    log_debug("Process finished. Stdout length: " + QString::number(output.length()) + ", Stderr: " + err);
    
    if (process.exitCode() != 0 || output.contains("Error:", Qt::CaseInsensitive) || output.contains("DbError", Qt::CaseInsensitive)) {
        pimpl_->last_error_ = !err.isEmpty() ? err : output;
        return result;
    }

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    log_debug("Lines count: " + QString::number(lines.size()));
    
    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i];
        QStringList parts = line.split(QRegularExpression("\\s{2,}"), Qt::SkipEmptyParts);
        if (parts.size() >= 1) {
            Container c;
            c.id = parts[0];
            c.image = parts.size() > 1 ? parts[1].trimmed() : "";
            c.name = parts.size() > 2 ? parts[2].trimmed() : "";
            c.status = parts.size() > 3 ? parts[3].trimmed() : "";
            c.created_at = parts.size() > 4 ? parts[4].trimmed() : "";
            result.push_back(c);
        }
    }
    log_debug("Parsed containers count: " + QString::number(result.size()));
    return result;
}

auto Backend::get_container_inspect(const QString& id) const -> QString {
    QString cli_path = resolve_cli_path();
    if (!QFile::exists(cli_path)) return "";

    QProcess process;
    process.start(cli_path, QStringList() << "inspect" << id);
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(500);
        return "";
    }
    if (process.exitCode() != 0) return "";
    return QString::fromUtf8(process.readAllStandardOutput());
}

auto Backend::get_container_top(const QString& id) const -> QString {
    QString cli_path = resolve_cli_path();
    if (!QFile::exists(cli_path)) return "";

    QProcess process;
    process.start(cli_path, QStringList() << "top" << id);
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(500);
        return "Error: Timeout";
    }
    
    QString out = QString::fromUtf8(process.readAllStandardOutput());
    QString err = QString::fromUtf8(process.readAllStandardError());
    
    if (out.isEmpty() && !err.isEmpty()) {
        return err; // Return error to see what went wrong
    }
    
    return out;
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
            args << QProcess::splitCommand(container.options);
        }
        
        if (container.cpu_quota > 0) args << "--cpu-quota" << QString::number(container.cpu_quota);
        if (container.cpu_weight > 0) args << "--cpu-weight" << QString::number(container.cpu_weight);
        if (!container.memory_max.isEmpty()) args << "--memory-max" << container.memory_max;
        if (!container.memory_swap.isEmpty()) args << "--memory-swap" << container.memory_swap;
        if (container.pids_limit > 0) args << "--pids-limit" << QString::number(container.pids_limit);
        if (!container.cpuset_cpus.isEmpty()) args << "--cpuset-cpus" << container.cpuset_cpus;
        if (!container.cpuset_mems.isEmpty()) args << "--set-cpuset-mems" << container.cpuset_mems;
        if (!container.io_weight.isEmpty()) args << "--set-io-weight" << container.io_weight;
        if (!container.io_max.isEmpty()) args << "--set-io-max" << container.io_max;
        
        // Interactive and Detach flags, and Image positional argument
        if (!container.prevent_interaction) {
            args << "-i";
        }
        args << "-d" << container.image;
        if (!container.command.isEmpty()) {
            args << QProcess::splitCommand(container.command);
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
        qDebug() << "Executing Quiver CLI for pause:" << cli_path << "pause" << container_ids;
        QProcess::startDetached(cli_path, QStringList() << "pause" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::unpause_container(const QStringList& container_ids) -> void {
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        qDebug() << "Executing Quiver CLI for unpause:" << cli_path << "unpause" << container_ids;
        QProcess::startDetached(cli_path, QStringList() << "unpause" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::start_container(const QStringList& container_ids) -> void {
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        qDebug() << "Executing Quiver CLI for start:" << cli_path << "start" << container_ids;
        QProcess::startDetached(cli_path, QStringList() << "start" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::update_container(const Container& container) -> void {
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QStringList args;
        args << "update" << container.id;
        
        if (container.cpu_quota > 0) args << "--cpu-quota" << QString::number(container.cpu_quota);
        if (container.cpu_weight > 0) args << "--cpu-weight" << QString::number(container.cpu_weight);
        if (!container.memory_max.isEmpty()) args << "--memory-max" << container.memory_max;
        if (!container.memory_swap.isEmpty()) args << "--memory-swap" << container.memory_swap;
        if (container.pids_limit > 0) args << "--pids-limit" << QString::number(container.pids_limit);
        if (!container.cpuset_cpus.isEmpty()) args << "--cpuset-cpus" << container.cpuset_cpus;
        if (!container.cpuset_mems.isEmpty()) args << "--set-cpuset-mems" << container.cpuset_mems;
        if (!container.io_weight.isEmpty()) args << "--set-io-weight" << container.io_weight;
        if (!container.io_max.isEmpty()) args << "--set-io-max" << container.io_max;

        qDebug() << "Executing Quiver CLI for update:" << cli_path << args;
        QProcess::startDetached(cli_path, args);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::stop_container(const QStringList& container_ids) -> void {
    if (container_ids.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        qDebug() << "Executing Quiver CLI for stop:" << cli_path << "stop" << container_ids;
        QProcess::startDetached(cli_path, QStringList() << "stop" << container_ids);
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::prune_containers() -> void {
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        qDebug() << "Executing Quiver CLI for prune:" << cli_path << "prune";
        QProcess::startDetached(cli_path, QStringList() << "prune");
    } else {
        qDebug() << "CLI not found at" << cli_path;
    }
}

auto Backend::get_images() const -> std::vector<Image> {
    std::vector<Image> result;
    QString cli_path = resolve_cli_path();
    if (!QFile::exists(cli_path)) return result;

    QProcess process;
    process.start(cli_path, QStringList() << "image" << "ls");
    if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished(500);
        return result;
    }

    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        
        // Truncate if we hit usage errors
        if (line.startsWith("Usage:") || line.startsWith("quiver image:")) {
            break; 
        }
        
        QStringList parts = line.split(QRegularExpression("\\s{2,}"), Qt::SkipEmptyParts);
        // Fallback to single space if double space parsing yields too few parts
        if (parts.size() < 5) {
            parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        }
        
        if (parts.size() >= 5) {
            Image img;
            img.id = parts[0];
            img.name = parts[1]; // CLI Name
            img.tag = parts[2];  // CLI Tag
            img.size = parts[3];
            img.source = parts[4];
            result.push_back(img);
        }
    }
    return result;
}
auto Backend::add_image(const Image& img) -> void { pimpl_->images_.push_back(img); }
auto Backend::delete_images(const QStringList& targets) -> void {
    if (targets.isEmpty()) return;
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QStringList args;
        args << "image" << "rm" << targets;
        qDebug() << "Executing Quiver CLI for delete images:" << cli_path << args;
        QProcess::startDetached(cli_path, args);
    }
}

auto Backend::pull_image(const QString& name, const QString& tag) -> void {
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QString target = name;
        if (!tag.isEmpty()) {
            target += ":" + tag;
        }
        QStringList args;
        args << "image" << "pull" << target;
        qDebug() << "Executing Quiver CLI for pull image:" << cli_path << args;
        QProcess::startDetached(cli_path, args);
    }
}

auto Backend::get_volumes() const -> std::vector<Volume> {
    std::vector<Volume> result;
    QString cli_path = resolve_cli_path();
    if (!QFile::exists(cli_path)) return result;

    QProcess process;
    process.start(cli_path, QStringList() << "volume" << "ls");
    if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished(500);
        return result;
    }

    QString output = process.readAllStandardOutput();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    // Skip header line
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        
        // Truncate if we hit "Usage:" or "quiver volume:" error messages
        if (line.startsWith("Usage:") || line.startsWith("quiver volume:")) {
            break; 
        }
        
        QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        // Only parse if it looks like a valid row with at least 4 parts and first part is long hash
        if (parts.size() >= 4 && parts[0].length() >= 12) {
            Volume v;
            v.container_id = parts[0];
            v.source = parts[1];
            v.destination = parts[2];
            v.type = parts[3];
            result.push_back(v);
        }
    }
    return result;
}
auto Backend::add_volume(const QString& container_id, const QString& host_path, const QString& container_path, const QString& mode) -> void {
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        QString mapping = host_path + ":" + container_path;
        if (!mode.isEmpty()) mapping += ":" + mode;
        QStringList args;
        args << "volume" << "add" << container_id << mapping;
        qDebug() << "Executing Quiver CLI for add volume:" << cli_path << args;
        QProcess::startDetached(cli_path, args);
    }
}
auto Backend::delete_volumes(const QMap<QString, QStringList>& targets) -> void {
    QString cli_path = resolve_cli_path();
    if (QFile::exists(cli_path)) {
        for (auto it = targets.constBegin(); it != targets.constEnd(); ++it) {
            QString container_id = it.key();
            QStringList paths = it.value();
            QStringList args;
            args << "volume" << "rm" << container_id;
            args.append(paths);
            qDebug() << "Executing Quiver CLI for rm volume:" << cli_path << args;
            QProcess::startDetached(cli_path, args);
        }
    }
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
