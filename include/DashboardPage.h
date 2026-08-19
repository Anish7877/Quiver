#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

namespace Quiver {

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);
    ~DashboardPage() override;

signals:
    void navigate_to_containers();
    void open_create_container();
    void open_create_container_with_config(const QJsonObject& config);

private slots:
    void on_configs_loaded(const QJsonArray& configs);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} 
