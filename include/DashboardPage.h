#pragma once
#include <QWidget>
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

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} 
