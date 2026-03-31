#pragma once
#include <QDialog>
#include <memory>

namespace Quiver {

class AuthWindow : public QDialog {
    Q_OBJECT
public:
    explicit AuthWindow(QWidget* parent = nullptr);
    ~AuthWindow() override;

private slots:
    void evaluate_password_strength(const QString& password);
    void handle_login();
    void handle_register();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}