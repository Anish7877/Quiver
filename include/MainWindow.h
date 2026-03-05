#pragma once
#include "common_header.hpp"
#include <QMainWindow>

class FlowLayout;

namespace Quiver {


class ImagesPage;
class VolumesPage;
class PortsPage;
class DevicesPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    auto toggle_sidebar() -> void;
    auto switch_tab(int index) -> void;
    auto toggle_theme() -> void;

private:
    auto setup_sidebar() -> void;
    auto setup_content() -> void;
    auto refresh_container_grid() -> void;
    auto update_sidebar_icons() -> void;
    struct Impl;
    std::unique_ptr<Impl> pimpl_ {};
};

}
