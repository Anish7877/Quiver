#pragma once
#include "common_header.hpp"
#include <QWidget>
#include <QString>
#include <memory>

namespace Quiver {

class ContainerDetailsPage : public QWidget {
    Q_OBJECT
public:
    explicit ContainerDetailsPage(QWidget* parent = nullptr);
    ~ContainerDetailsPage() override;

    auto set_container_id(const QString& id) -> void;
    auto refresh() -> void;

signals:
    void back_requested();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace Quiver
