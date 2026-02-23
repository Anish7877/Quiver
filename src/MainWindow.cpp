#include "include/MainWindow.h"
#include "include/Components.h"
#include "include/FlowLayout.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFile>
#include <QApplication>
#include <QButtonGroup>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPixmap>
#include <QStyle>
#include <QRandomGenerator>
#include <QPainter>
#include <QIcon>
#include <QFrame>
#include <QPushButton>
#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QList>

namespace Quiver {

struct MainWindow::Impl {
    QWidget* central_widget_ {};
    QFrame* sidebar_ {};
    QVBoxLayout* sidebar_layout_ {};

    QStackedWidget* main_stack_ {};
    QWidget* dashboard_page_ {};
    QWidget* containers_page_ {};
    QWidget* images_page_ {};
    QWidget* volumes_page_ {};
    QWidget* ports_page_ {};
    QWidget* devices_page_ {};

    QScrollArea* scroll_area_ {};
    FlowLayout* container_grid_ {};

    QPushButton* create_btn_ {};
    QPushButton* theme_btn_ {};

    bool is_sidebar_expanded_ { true };
    bool is_dark_mode_ { true };
};


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), pimpl_{std::make_unique<Impl>()}
{
    resize(1280, 850);
    setWindowTitle("Quiver Platform - C++ Edition");

    QFile file(":/assets/style.qss");
    if(file.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(file.readAll());
    }

    auto* central { new QWidget };
    setCentralWidget(central);

    auto* root { new QHBoxLayout(central) };
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    setup_sidebar();
    root->addWidget(pimpl_->sidebar_);

    setup_content();
    root->addWidget(pimpl_->central_widget_);
}

MainWindow::~MainWindow() = default;

auto MainWindow::setup_sidebar() -> void {
    pimpl_->sidebar_ = new QFrame;
    pimpl_->sidebar_->setObjectName("Sidebar");
    pimpl_->sidebar_->setFixedWidth(240);

    pimpl_->sidebar_layout_ = new QVBoxLayout(pimpl_->sidebar_);
    pimpl_->sidebar_layout_->setContentsMargins(10, 20, 10, 20);
    pimpl_->sidebar_layout_->setSpacing(5);

    auto* toggle_btn { new QPushButton("  QUIVER") };
    toggle_btn->setObjectName("ToggleBtn");
    toggle_btn->setProperty("iconPath", ":/assets/icons/menu.svg");
    toggle_btn->setProperty("navText", "  QUIVER");
    toggle_btn->setProperty("expanded", true);
    toggle_btn->setCursor(Qt::PointingHandCursor);
    connect(toggle_btn, &QPushButton::clicked, this, &MainWindow::toggle_sidebar);

    pimpl_->sidebar_layout_->addWidget(toggle_btn);
    pimpl_->sidebar_layout_->addSpacing(30);

    auto* nav_group { new QButtonGroup(this) };
    nav_group->setExclusive(true);

    auto add_nav = [&](const QString& icon_path, const QString& text, int index, bool active) {
        auto* btn { new QPushButton("  " + text) };
        btn->setObjectName("NavButton");
        btn->setProperty("iconPath", icon_path);
        btn->setProperty("navText", "  " + text);
        btn->setProperty("expanded", true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);

        if(active) {
            btn->setChecked(true);
        }
        connect(btn, &QPushButton::clicked, this, [this, index](){ switch_tab(index); });
        nav_group->addButton(btn);
        pimpl_->sidebar_layout_->addWidget(btn);
    };

    add_nav(":/assets/icons/home.svg", "Home", 0, false);
    add_nav(":/assets/icons/containers.svg", "Containers", 1, true);
    add_nav(":/assets/icons/images.svg", "Images", 2, false);
    add_nav(":/assets/icons/volumes.svg", "Volumes", 3, false);
    add_nav(":/assets/icons/ports.svg", "Ports", 4, false);
    add_nav(":/assets/icons/devices.svg", "Devices", 5, false);
    pimpl_->sidebar_layout_->addStretch();

    pimpl_->theme_btn_ = new QPushButton;
    pimpl_->theme_btn_->setObjectName("ThemeBtn");
    pimpl_->theme_btn_->setProperty("iconPath", pimpl_->is_dark_mode_ ? ":/assets/icons/moon.svg" : ":/assets/icons/sun.svg");
    pimpl_->theme_btn_->setProperty("navText", pimpl_->is_dark_mode_ ? "  Dark Mode" : "  Light Mode");
    pimpl_->theme_btn_->setText(pimpl_->is_sidebar_expanded_ ? pimpl_->theme_btn_->property("navText").toString() : "");
    pimpl_->theme_btn_->setProperty("expanded", true);
    pimpl_->theme_btn_->setCursor(Qt::PointingHandCursor);

    connect(pimpl_->theme_btn_, &QPushButton::clicked, this, &MainWindow::toggle_theme);
    pimpl_->sidebar_layout_->addWidget(pimpl_->theme_btn_);

    update_sidebar_icons();
}

auto MainWindow::setup_content() -> void {
    pimpl_->central_widget_ = new QWidget;
    auto* layout { new QVBoxLayout(pimpl_->central_widget_) };
    layout->setContentsMargins(40, 40, 40, 0);

    pimpl_->main_stack_ = new QStackedWidget;
    layout->addWidget(pimpl_->main_stack_);

    // 0: Dashboard
    pimpl_->dashboard_page_ = new QWidget;
    auto* d_layout { new QVBoxLayout(pimpl_->dashboard_page_) }; d_layout->setAlignment(Qt::AlignCenter);
    auto* d_label { new QLabel("Dashboard View (Coming Soon)") };
    d_label->setStyleSheet("font-size: 24px; color: #64748B; font-weight: bold;");
    d_layout->addWidget(d_label);
    pimpl_->main_stack_->addWidget(pimpl_->dashboard_page_);

    // 1: Containers
    pimpl_->containers_page_ = new QWidget;
    auto* c_layout { new QVBoxLayout(pimpl_->containers_page_) };
    c_layout->setContentsMargins(0, 0, 0, 0);
    c_layout->setSpacing(30);

    auto* stats_row { new QHBoxLayout }; stats_row->setSpacing(20);
    stats_row->addWidget(new StatCard("TOTAL", "3", "#ffffff"));
    stats_row->addWidget(new StatCard("RUNNING", "2", "#4ade80"));
    stats_row->addWidget(new StatCard("STOPPED", "1", "#fb7185"));
    stats_row->addWidget(new StatCard("SYSTEM LOAD", "27%", "#ffffff"));
    c_layout->addLayout(stats_row);

    auto* header { new QHBoxLayout };
    auto* title { new QLabel("Containers") }; title->setObjectName("PageTitle");
    pimpl_->create_btn_ = new QPushButton("+ CREATE NEW");
    pimpl_->create_btn_->setObjectName("PrimaryButton");
    pimpl_->create_btn_->setCursor(Qt::PointingHandCursor);

    connect(pimpl_->create_btn_, &QPushButton::clicked, this, [this](){
        CreateDialog d(this);
        if(d.exec() == QDialog::Accepted) {
            QString name { d.get_container_name() };
            QString img { d.get_container_image() };
            if(name.trimmed().isEmpty()) name = "new-container";
            if(img.trimmed().isEmpty()) img = "ubuntu:latest";
            QString id { QString::number(QRandomGenerator::global()->generate(), 16).right(6) };

            Backend::get_instance().add_container({id, name, img, "running"});
            refresh_container_grid();
        }
    });

    header->addWidget(title); header->addStretch(); header->addWidget(pimpl_->create_btn_);
    c_layout->addLayout(header);

    pimpl_->scroll_area_ = new QScrollArea;
    pimpl_->scroll_area_->setWidgetResizable(true);
    pimpl_->scroll_area_->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto* grid_c { new QWidget };
    grid_c->setObjectName("GridContainer");
    grid_c->setStyleSheet("QWidget#GridContainer { background: transparent; }");

    pimpl_->container_grid_ = new ::FlowLayout(grid_c, 0, 20, 20);

    refresh_container_grid();
    pimpl_->scroll_area_->setWidget(grid_c);
    c_layout->addWidget(pimpl_->scroll_area_);
    pimpl_->main_stack_->addWidget(pimpl_->containers_page_);

    auto create_placeholder = [&](QWidget*& page, const QString& text) {
        page = new QWidget;
        auto* l { new QVBoxLayout(page) }; l->setAlignment(Qt::AlignCenter);
        auto* lbl { new QLabel(text) }; lbl->setStyleSheet("font-size: 24px; color: #64748B; font-weight: bold;");
        l->addWidget(lbl);
        pimpl_->main_stack_->addWidget(page);
    };

    create_placeholder(pimpl_->images_page_, "Images Management (Coming Soon)");
    create_placeholder(pimpl_->volumes_page_, "Volumes Management (Coming Soon)");
    create_placeholder(pimpl_->ports_page_, "Ports Management (Coming Soon)");
    create_placeholder(pimpl_->devices_page_, "Devices Management (Coming Soon)");

    pimpl_->main_stack_->setCurrentIndex(1);
}


auto MainWindow::toggle_sidebar() -> void {
    bool will_expand { !pimpl_->is_sidebar_expanded_ };
    int start { will_expand ? 80 : 240 };
    int end { will_expand ? 240 : 80 };

    auto* anim_group { new QParallelAnimationGroup(this) };
    auto* min_anim { new QPropertyAnimation(pimpl_->sidebar_, "minimumWidth") };
    min_anim->setDuration(300); min_anim->setStartValue(start); min_anim->setEndValue(end); min_anim->setEasingCurve(QEasingCurve::InOutQuad);
    auto* max_anim { new QPropertyAnimation(pimpl_->sidebar_, "maximumWidth") };
    max_anim->setDuration(300); max_anim->setStartValue(start); max_anim->setEndValue(end); max_anim->setEasingCurve(QEasingCurve::InOutQuad);

    anim_group->addAnimation(min_anim);
    anim_group->addAnimation(max_anim);
    anim_group->start(QAbstractAnimation::DeleteWhenStopped);

    QList<QPushButton*> btns { pimpl_->sidebar_->findChildren<QPushButton*>() };
    for(auto* btn : btns) {
        if(btn->property("navText").isValid()) {
            btn->setProperty("expanded", will_expand);
            btn->setText(will_expand ? btn->property("navText").toString() : "");
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
        }
    }
    pimpl_->is_sidebar_expanded_ = will_expand;
}

auto MainWindow::toggle_theme() -> void {
    QPixmap pixmap { this->grab() };
    auto* overlay { new QLabel(this) };
    overlay->setPixmap(pixmap);
    overlay->setGeometry(this->rect());
    overlay->show();
    overlay->raise();

    pimpl_->is_dark_mode_ = !pimpl_->is_dark_mode_;

    QString theme_path { pimpl_->is_dark_mode_ ? ":/assets/style.qss" : ":/assets/light_style.qss" };
    QFile file(theme_path);
    if(file.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(file.readAll());
        file.close();
    }

    if (pimpl_->is_dark_mode_) {
        pimpl_->theme_btn_->setProperty("iconPath", ":/assets/icons/moon.svg");
        pimpl_->theme_btn_->setProperty("navText", "  Dark Mode");
    } else {
        pimpl_->theme_btn_->setProperty("iconPath", ":/assets/icons/sun.svg");
        pimpl_->theme_btn_->setProperty("navText", "  Light Mode");
    }
    pimpl_->theme_btn_->setText(pimpl_->is_sidebar_expanded_ ? pimpl_->theme_btn_->property("navText").toString() : "");

    update_sidebar_icons();

    auto* eff { new QGraphicsOpacityEffect(overlay) };
    overlay->setGraphicsEffect(eff);
    auto* a { new QPropertyAnimation(eff, "opacity") };
    a->setDuration(300); a->setStartValue(1.0); a->setEndValue(0.0);
    connect(a, &QPropertyAnimation::finished, overlay, &QLabel::deleteLater);
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

auto MainWindow::switch_tab(int index) -> void {
    if(index >= 0 && index < pimpl_->main_stack_->count()) {
        pimpl_->main_stack_->setCurrentIndex(index);
        update_sidebar_icons();
    }
}

auto MainWindow::update_sidebar_icons() -> void {
    QColor normal_color { pimpl_->is_dark_mode_ ? QColor("#94A3B8") : QColor("#64748B") };
    QColor active_color { pimpl_->is_dark_mode_ ? QColor("#3B82F6") : QColor("#2563EB") };

    QList<QPushButton*> btns { pimpl_->sidebar_->findChildren<QPushButton*>() };
    for(auto* btn : btns) {
        QString path { btn->property("iconPath").toString() };
        if(!path.isEmpty()) {
            QColor target_color { normal_color };
            if (btn->isCheckable() && btn->isChecked()) {
                target_color = active_color;
            }

            QPixmap pixmap(path);
            if (!pixmap.isNull()) {
                pixmap = pixmap.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QPainter painter(&pixmap);
                painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                painter.fillRect(pixmap.rect(), target_color);
                painter.end();
                btn->setIcon(QIcon(pixmap));
                btn->setIconSize(QSize(20, 20));
            }
        }
    }
}

auto MainWindow::refresh_container_grid() -> void {
    QLayoutItem *child;
    while ((child = pimpl_->container_grid_->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    auto data { Backend::get_instance().get_containers() };
    for(const auto& c : data) {
        auto* card { new ContainerCard(c) };
        connect(card, &ContainerCard::state_changed, this, &MainWindow::refresh_container_grid);
        pimpl_->container_grid_->addWidget(card);
    }
}

}
