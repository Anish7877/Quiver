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
                       QWidget*           parent = nullptr,
                       bool               has_actions = true);
    ~TablePage() override;

    auto add_row(const QStringList& row_data,
                 const QString&     action_label = QString(),
                 const QString&     action_obj_name = QString()) -> void;

    auto table() -> QTableWidget*;
    auto set_action_column_width(int width) -> void;
    auto clear_rows() -> void;
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
    auto refresh() -> void;
protected:
    void resizeEvent(QResizeEvent* event) override;
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
    auto open_create_dialog() -> void;

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
    auto refresh() -> void;
protected:
    void resizeEvent(QResizeEvent* event) override;
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
