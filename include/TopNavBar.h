#pragma once
#include "common_header.hpp"
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

namespace Quiver {

class TopNavBar : public QFrame {
    Q_OBJECT
public:
    explicit TopNavBar(QWidget* parent = nullptr);
    ~TopNavBar() override;

    auto update_logo_width(int sidebar_width) -> void;

signals:
    auto settings_clicked() -> void;
    auto search_changed(const QString& text) -> void;
    auto logo_clicked() -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

}
