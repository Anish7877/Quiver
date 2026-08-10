#pragma once
#include "common_header.hpp"
#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QHBoxLayout>

namespace Quiver {

class TablePage : public QWidget {
    Q_OBJECT
public:
    explicit TablePage(const QString&     title,
                       const QStringList& columns,
                       QWidget*           parent = nullptr);
    ~TablePage() override;

    auto add_row(const QStringList& row_data,
                 const QString&     action_label,
                 const QString&     action_obj_name) -> void;

    auto table() -> QTableWidget*;
    auto header_layout() -> QHBoxLayout*;

signals:
    auto add_clicked() -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class ImagesPage : public QWidget {
    Q_OBJECT
public:
    explicit ImagesPage(QWidget* parent = nullptr);
    ~ImagesPage() override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class ContainersPage : public QWidget {
    Q_OBJECT
public:
    explicit ContainersPage(QWidget* parent = nullptr);
    ~ContainersPage() override;
    
    auto refresh() -> void;

signals:
    void container_info_requested(const QString& container_id);

protected:
    void resizeEvent(QResizeEvent* event) override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class VolumesPage : public QWidget {
    Q_OBJECT
public:
    explicit VolumesPage(QWidget* parent = nullptr);
    ~VolumesPage() override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class PortsPage : public QWidget {
    Q_OBJECT
public:
    explicit PortsPage(QWidget* parent = nullptr);
    ~PortsPage() override;
    
    auto refresh() -> void;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class DevicesPage : public QWidget {
    Q_OBJECT
public:
    explicit DevicesPage(QWidget* parent = nullptr);
    ~DevicesPage() override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class SettingsPage : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);
    ~SettingsPage() override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

}
