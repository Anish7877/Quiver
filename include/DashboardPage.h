#pragma once
#include <QWidget>
#include <memory>

namespace Quiver {

class DashboardPage : public QWidget {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);
    ~DashboardPage() override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} 
