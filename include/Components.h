#pragma once
#include "common_header.hpp"
#include "Backend.h"

#include <QWidget>
#include <QFrame>
#include <QPushButton>
#include <QLabel>
#include <QDialog>
#include <QLineEdit>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QStackedWidget>
#include <QListWidget>
#include <QMenu>
#include <QTableWidget>

namespace Quiver {


class ToggleSwitch : public QCheckBox {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    ~ToggleSwitch() override;
protected:
    auto paintEvent(QPaintEvent* event) -> void override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};


class ActivityGraph : public QWidget {
    Q_OBJECT
public:
    explicit ActivityGraph(QWidget* parent = nullptr);
    ~ActivityGraph() override;
protected:
    auto paintEvent(QPaintEvent* event) -> void override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};


// ContainerCard has been replaced by ContainersPage in TablePages


class StatCard : public QFrame {
    Q_OBJECT
public:
    explicit StatCard(const QString& title, const QString& value,
                      const QString& color, QWidget* parent = nullptr);
    ~StatCard() override;
    
    auto set_value(const QString& val) -> void;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};


class ResourceTable : public QTableWidget {
    Q_OBJECT
public:
    explicit ResourceTable(const QStringList& headers, QWidget* parent = nullptr);
    ~ResourceTable() override;

    auto add_status_badge(int row, int col, const QString& status) -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};


class CreateDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateDialog(QWidget* parent = nullptr);
    ~CreateDialog() override;

    auto get_container_name() const -> QString;
    auto get_container_image() const -> QString;
    auto get_devices() const -> QStringList;
    auto get_volumes() const -> QStringList;
    auto get_ports() const -> QStringList;
    auto get_filesystem() const -> QString;
    auto get_prevent_interaction() const -> bool;
    auto get_command() const -> QString;
    auto get_options() const -> QString;

private slots:
    auto show_visual() -> void;
    auto show_json() -> void;
    auto on_add_device() -> void;
    auto on_add_volume() -> void;
    auto on_add_port() -> void;
    auto on_import_json() -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class DeleteDialog : public QDialog {
    Q_OBJECT
public:
    explicit DeleteDialog(const QString& container_name, QWidget* parent = nullptr);
    ~DeleteDialog() override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class CustomAlert : public QDialog {
    Q_OBJECT
public:
    enum Type {
        Warning,
        Question
    };
    explicit CustomAlert(Type type, const QString& title, const QString& message, QWidget* parent = nullptr);
    ~CustomAlert() override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

class CircularGauge : public QWidget {
    Q_OBJECT
public:
    explicit CircularGauge(const QString& title, double soft_limit, double hard_limit, QWidget* parent = nullptr);
    ~CircularGauge() override;
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

}
