#include "include/TopNavBar.h"
#include <QHBoxLayout>
#include <QPixmap>
#include <QIcon>

namespace Quiver {

struct TopNavBar::Impl {
    QWidget*     logo_area_    {};
    QPushButton* logo_btn_     {};
    QLabel*      app_title_    {};
    QLineEdit*   search_bar_   {};
    QPushButton* settings_btn_ {};
};

TopNavBar::TopNavBar(QWidget* parent)
    : QFrame(parent), pimpl_{ std::make_unique<Impl>() }
{
    setObjectName("TopNavBar");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(52);

    auto* layout { new QHBoxLayout(this) };
    layout->setContentsMargins(0, 0, 20, 0);
    layout->setSpacing(0);


    pimpl_->logo_area_ = new QWidget;
    pimpl_->logo_area_->setFixedWidth(220);
    pimpl_->logo_area_->setObjectName("LogoArea");

    auto* logo_layout { new QHBoxLayout(pimpl_->logo_area_) };
    logo_layout->setContentsMargins(14, 0, 14, 0);
    logo_layout->setSpacing(10);

    pimpl_->logo_btn_ = new QPushButton;
    pimpl_->logo_btn_->setObjectName("LogoBtn");
    pimpl_->logo_btn_->setFixedSize(32, 32);
    pimpl_->logo_btn_->setCursor(Qt::PointingHandCursor);
    pimpl_->logo_btn_->setToolTip("Toggle sidebar");

    QPixmap logo_px(":/assets/icons/logo.png");
    if(!logo_px.isNull()) {
        logo_px = logo_px.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pimpl_->logo_btn_->setIcon(QIcon(logo_px));
        pimpl_->logo_btn_->setIconSize(QSize(28, 28));
    }
    connect(pimpl_->logo_btn_, &QPushButton::clicked,
            this, &TopNavBar::logo_clicked);

    pimpl_->app_title_ = new QLabel("QUIVER");
    pimpl_->app_title_->setObjectName("AppTitle");

    logo_layout->addWidget(pimpl_->logo_btn_);
    logo_layout->addWidget(pimpl_->app_title_);
    logo_layout->addStretch();
    layout->addWidget(pimpl_->logo_area_);

    auto* sep { new QFrame };
    sep->setFrameShape(QFrame::VLine);
    sep->setObjectName("NavSep");
    layout->addWidget(sep);

    layout->addStretch();


    pimpl_->search_bar_ = new QLineEdit;
    pimpl_->search_bar_->setObjectName("SearchBar");
    pimpl_->search_bar_->setPlaceholderText("Search containers, images, volumes...");
    pimpl_->search_bar_->setClearButtonEnabled(true);
    connect(pimpl_->search_bar_, &QLineEdit::textChanged,
            this, &TopNavBar::search_changed);
    layout->addWidget(pimpl_->search_bar_);

    layout->addStretch();


    pimpl_->settings_btn_ = new QPushButton("SETTINGS");
    pimpl_->settings_btn_->setObjectName("NavSettingsBtn");
    pimpl_->settings_btn_->setCheckable(true);
    pimpl_->settings_btn_->setCursor(Qt::PointingHandCursor);
    connect(pimpl_->settings_btn_, &QPushButton::clicked,
            this, &TopNavBar::settings_clicked);
    layout->addWidget(pimpl_->settings_btn_);
}

TopNavBar::~TopNavBar() = default;

auto TopNavBar::update_logo_width(int sidebar_width) -> void {
    pimpl_->logo_area_->setFixedWidth(sidebar_width);

    pimpl_->app_title_->setVisible(sidebar_width > 80);
}

}
