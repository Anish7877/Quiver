#include "include/MainWindow.h"
#include "include/Components.h"
#include "include/TablePages.h"
#include "include/FlowLayout.h"
#include <QDesktopServices>
#include <QUrl>
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
#include <QPainterPath>
#include <QIcon>
#include <QFrame>
#include <QPushButton>
#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QList>
#include "include/DashboardPage.h"
#include "include/ContainerDetailsPage.h"
#include "include/AuthManager.h"
#include <QProcess> 
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QAbstractAxis>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDir>
#include <QCoreApplication>
#include <QTimer>

namespace Quiver {

struct MainWindow::Impl {
    QWidget*        central_widget_  {};
    QFrame*         sidebar_         {};
    QVBoxLayout*    sidebar_layout_  {};

    QStackedWidget* main_stack_      {};
    QWidget*        dashboard_page_  {};
    ContainersPage* containers_page_ {};
    ImagesPage*     images_page_     {};
    VolumesPage*    volumes_page_    {};
    PortsPage*      ports_page_      {};
    DevicesPage*    devices_page_    {};
    ContainerDetailsPage* details_page_ {};
    QWidget* auth_page_       {}; 
    QFrame* top_bar_         {}; 
    QScrollArea*    scroll_area_     {};
    FlowLayout*     container_grid_  {};
    
    StatCard* stat_total_ {};
    StatCard* stat_running_ {};
    StatCard* stat_stopped_ {};

    QPushButton*    create_btn_      {};
    QPushButton*    theme_btn_       {};
    QButtonGroup* nav_group_        {};
    QPushButton* settings_btn_    {}; 
    SettingsPage* settings_page_   {}; 

    bool is_sidebar_expanded_ { true  };
    bool is_dark_mode_        { true  };


    QLabel* logo_label_ {};
    QProcess* backend_process_ {};
    QNetworkAccessManager* network_manager_ {};
};

static QString resolve_path(const QString& relative_path) {
    QDir dir(QCoreApplication::applicationDirPath());
    // Try to find the root QuiverGUI directory
    while (dir.dirName() != "QuiverGUI" && !dir.isRoot()) {
        dir.cdUp();
    }
    return QDir::cleanPath(dir.absoluteFilePath(relative_path));
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), pimpl_{std::make_unique<Impl>()}
{
    resize(1280, 850);
    setWindowTitle("Quiver");

    QFile file(":/assets/style.qss");
    if (file.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(file.readAll());
    }

    auto* central { new QWidget };
    setCentralWidget(central);

    pimpl_->network_manager_ = new QNetworkAccessManager(this);
    check_and_start_backend();

    auto* root { new QHBoxLayout(central) };
    root->setSpacing(0);
    root->setContentsMargins(0, 0, 0, 0);

    setup_sidebar();
    root->addWidget(pimpl_->sidebar_);

    setup_content();
    root->addWidget(pimpl_->central_widget_);

    
    if (!AuthManager::get_instance().is_logged_in()) {
        pimpl_->sidebar_->hide();
        pimpl_->top_bar_->hide();
        pimpl_->main_stack_->setCurrentWidget(pimpl_->auth_page_);
    } else {
        AuthManager::get_instance().fetch_profile();
        pimpl_->main_stack_->setCurrentIndex(1); 
    }

   
    connect(&AuthManager::get_instance(), &AuthManager::login_success, this, [this]() {
        pimpl_->sidebar_->show();
        pimpl_->top_bar_->show();
        pimpl_->main_stack_->setCurrentIndex(1); 
    });

   
    connect(&AuthManager::get_instance(), &AuthManager::logged_out, this, [this]() {
        pimpl_->sidebar_->hide();
        pimpl_->top_bar_->hide();
        pimpl_->main_stack_->setCurrentWidget(pimpl_->auth_page_);
    });
    
    auto* refresh_timer = new QTimer(this);
    connect(refresh_timer, &QTimer::timeout, this, [this]() {
        if (pimpl_->main_stack_ && pimpl_->containers_page_ && 
            pimpl_->main_stack_->currentWidget() == pimpl_->containers_page_) {
            refresh_container_grid();
        } else if (pimpl_->main_stack_ && pimpl_->ports_page_ && 
                   pimpl_->main_stack_->currentWidget() == pimpl_->ports_page_) {
            pimpl_->ports_page_->refresh();
        }
    });
    refresh_timer->start(10000);
}

MainWindow::~MainWindow() {
    if (pimpl_->backend_process_ && pimpl_->backend_process_->state() == QProcess::Running) {
        pimpl_->backend_process_->terminate();
        pimpl_->backend_process_->waitForFinished(1000);
    }
}

auto MainWindow::check_and_start_backend() -> void {
    QNetworkRequest request(QUrl("http://localhost:8080/health"));
    QNetworkReply* reply = pimpl_->network_manager_->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "Backend not reachable, starting it manually...";
            pimpl_->backend_process_ = new QProcess(this);
            QString backend_dir = resolve_path("quiver-backend");
            pimpl_->backend_process_->setWorkingDirectory(backend_dir);
            
            pimpl_->backend_process_->start("sh", QStringList() << "-c" << "/usr/local/go/bin/go run main.go || go run main.go");
            
            connect(pimpl_->backend_process_, &QProcess::readyReadStandardOutput, this, [this]() {
                qDebug() << "Backend (stdout):" << pimpl_->backend_process_->readAllStandardOutput();
            });
            connect(pimpl_->backend_process_, &QProcess::readyReadStandardError, this, [this]() {
                qDebug() << "Backend (stderr):" << pimpl_->backend_process_->readAllStandardError();
            });
            connect(pimpl_->backend_process_, &QProcess::errorOccurred, this, [](QProcess::ProcessError error) {
                if (error == QProcess::Crashed) {
                    qDebug() << "Go backend shut down successfully (terminated by GUI).";
                } else {
                    qDebug() << "Failed to start Go backend. Error code:" << error;
                }
            });
        } else {
            qDebug() << "Backend is already running.";
        }
    });
}

auto MainWindow::setup_sidebar() -> void {
    pimpl_->sidebar_ = new QFrame;
    pimpl_->sidebar_->setObjectName("Sidebar");
    pimpl_->sidebar_->setFixedWidth(240);

    pimpl_->sidebar_layout_ = new QVBoxLayout(pimpl_->sidebar_);
    pimpl_->sidebar_layout_->setContentsMargins(10, 20, 10, 15);
    pimpl_->sidebar_layout_->setSpacing(5);

    
    auto* logo_btn { new QPushButton };
    logo_btn->setObjectName("ToggleBtn");
    logo_btn->setProperty("iconPath", ":/assets/icons/Quiver.svg");
    logo_btn->setProperty("navText", "  QUIVER");
    logo_btn->setProperty("expanded", true);
    logo_btn->setCursor(Qt::PointingHandCursor);
    logo_btn->setFixedHeight(50);

    QPixmap logo_px(":/assets/icons/Quiver");
    if (logo_px.isNull()) logo_px = QPixmap(":/assets/icons/Quiver.svg");
    if (!logo_px.isNull()) {
        logo_px = logo_px.scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        logo_btn->setIcon(QIcon(logo_px));
        logo_btn->setIconSize(QSize(28, 28));
    }
    logo_btn->setText(" QUIVER");
    connect(logo_btn, &QPushButton::clicked, this, &MainWindow::toggle_sidebar);

    pimpl_->sidebar_layout_->addWidget(logo_btn);
    pimpl_->sidebar_layout_->addSpacing(25);

    pimpl_->nav_group_ = new QButtonGroup(this);
    pimpl_->nav_group_->setExclusive(true);

    auto add_nav = [&](const QString& icon_path, const QString& text, int index, bool active) {
        auto* btn { new QPushButton("  " + text) };
        btn->setObjectName("NavButton");
        btn->setProperty("iconPath", icon_path);
        btn->setProperty("navText", "  " + text);
        btn->setProperty("expanded", true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);
        if (active) btn->setChecked(true);
        
        connect(btn, &QPushButton::clicked, this, [this, index](){ 
            if (pimpl_->settings_btn_) pimpl_->settings_btn_->setChecked(false); 
            switch_tab(index); 
        });
        
        pimpl_->nav_group_->addButton(btn);
        pimpl_->sidebar_layout_->addWidget(btn);
    };

    add_nav(":/assets/icons/home.svg",       "Home",       0, false);
    add_nav(":/assets/icons/containers.svg", "Containers", 1, true );
    add_nav(":/assets/icons/images.svg",     "Images",     2, false);
    add_nav(":/assets/icons/volumes.svg",    "Volumes",    3, false);
    add_nav(":/assets/icons/ports.svg",      "Ports",      4, false);
    add_nav(":/assets/icons/devices.svg",    "Devices",    5, false);

    pimpl_->sidebar_layout_->addStretch(); 

    auto create_dot_icon = [](const QColor& color) -> QIcon {
        QPixmap pix(24, 24);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(6, 6, 10, 10); // Center a 10x10 dot in a 24x24 box
        return QIcon(pix);
    };

    auto* cli_status_btn { new QPushButton("  CLI: Offline") };
    cli_status_btn->setObjectName("CliStatusBtn");
    cli_status_btn->setProperty("expanded", true);
    cli_status_btn->setStyleSheet("text-align: left; padding: 5px; color: #a1a1aa; background: transparent; border: none; font-size: 13px; font-weight: bold;");
    
    QString cli_path = resolve_path("Quiver/Quiver/build/release/quiver");
    if (QFile::exists(cli_path)) {
        cli_status_btn->setIcon(create_dot_icon(QColor("#22c55e"))); 
        cli_status_btn->setText("  CLI: Live");
        cli_status_btn->setProperty("navText", "  CLI: Live");
    } else {
        cli_status_btn->setIcon(create_dot_icon(QColor("#ef4444"))); 
        cli_status_btn->setText("  CLI: Offline");
        cli_status_btn->setProperty("navText", "  CLI: Offline");
        
        QTimer::singleShot(2000, this, [this, cli_path]() {
            CustomAlert alert(CustomAlert::Warning, "Quiver CLI Not Found", 
                "The Quiver CLI binary was not found at:\n" + cli_path + 
                "\n\nPlease ensure you have compiled it.", this);
            alert.exec();
        });
    }
    pimpl_->sidebar_layout_->addWidget(cli_status_btn);


    auto* help_btn { new QPushButton("  Help") };
    help_btn->setObjectName("NavButton");
    help_btn->setProperty("iconPath", ":/assets/icons/help.svg");
    help_btn->setProperty("navText", "  Help");
    help_btn->setProperty("expanded", true);
    help_btn->setCursor(Qt::PointingHandCursor);
    pimpl_->sidebar_layout_->addWidget(help_btn);

  
    auto* docs_btn { new QPushButton("  Documentation") };
    docs_btn->setObjectName("NavButton");
    docs_btn->setProperty("iconPath", ":/assets/icons/docs.svg");
    docs_btn->setProperty("navText", "  Documentation");
    docs_btn->setProperty("expanded", true);
    docs_btn->setCursor(Qt::PointingHandCursor);
    connect(docs_btn, &QPushButton::clicked, this, []() {
        
        QDesktopServices::openUrl(QUrl("https://quiver-containers.docs.com")); 
    });
    pimpl_->sidebar_layout_->addWidget(docs_btn);

    auto* divider { new QFrame };
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet("background-color: #27272A; max-height: 1px; margin: 10px 0px;");
    pimpl_->sidebar_layout_->addWidget(divider);

    


    auto get_circular_icon = [](const QString& path, int size) -> QIcon {
        QPixmap src(path);
        if (src.isNull()) src.load(":/assets/icons/profile.svg");
        
        int hr_size = size * 2;
        QPixmap scaled = src.scaled(hr_size, hr_size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QPixmap cropped = scaled.copy((scaled.width() - hr_size) / 2, (scaled.height() - hr_size) / 2, hr_size, hr_size);
        
        QPixmap out(hr_size, hr_size);
        out.fill(Qt::transparent);
        
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath path_clip;
        path_clip.addEllipse(1, 1, hr_size - 2, hr_size - 2);
        p.setClipPath(path_clip);
        p.drawPixmap(0, 0, cropped);
        p.end();
        
        QPixmap final_pix = out.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return QIcon(final_pix);
    };

 
    QString display_name = AuthManager::get_instance().get_full_name();
    QString current_avatar = AuthManager::get_instance().get_cached_avatar_path();

    auto* profile_btn { new QPushButton("  " + display_name) };
    profile_btn->setObjectName("ProfileBtn");
    profile_btn->setProperty("navText", "  " + display_name);
    profile_btn->setProperty("expanded", true);
    profile_btn->setCursor(Qt::PointingHandCursor);
    

    
    profile_btn->setIcon(get_circular_icon(current_avatar, 64));
    profile_btn->setIconSize(QSize(26, 26)); 
    pimpl_->sidebar_layout_->addWidget(profile_btn);


    connect(&AuthManager::get_instance(), &AuthManager::profile_updated, this, [profile_btn, get_circular_icon]() {
        QString new_name = AuthManager::get_instance().get_username();
        profile_btn->setProperty("navText", "  " + new_name);
        
        QString avatar_path = AuthManager::get_instance().get_cached_avatar_path();
        

        profile_btn->setIcon(get_circular_icon(avatar_path, 64));
        
        profile_btn->style()->unpolish(profile_btn);
        profile_btn->style()->polish(profile_btn);
        
        if (profile_btn->property("expanded").toBool()) {
            profile_btn->setText("  " + new_name);
        }
    });


    auto* auth_btn { new QPushButton("  Logout") };
    auth_btn->setObjectName("AuthBtn");
    auth_btn->setProperty("iconPath", ":/assets/icons/logout.svg");
    auth_btn->setProperty("navText", "  Logout");
    auth_btn->setProperty("expanded", true);
    auth_btn->setCursor(Qt::PointingHandCursor);
    auth_btn->setIcon(QIcon(":/assets/icons/logout.svg")); 
    auth_btn->setIconSize(QSize(24, 24));
    pimpl_->sidebar_layout_->addWidget(auth_btn);


    connect(auth_btn, &QPushButton::clicked, this, [this]() {
        AuthManager::get_instance().logout();
        qApp->quit();
        QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
    });

    update_sidebar_icons();

}

auto MainWindow::setup_content() -> void {


    pimpl_->central_widget_ = new QWidget;
    pimpl_->central_widget_->setObjectName("CentralWidget");
    auto* main_v_layout { new QVBoxLayout(pimpl_->central_widget_) };
    main_v_layout->setContentsMargins(0, 0, 0, 0);
    main_v_layout->setSpacing(0);

    pimpl_->top_bar_ = new QFrame;
    pimpl_->top_bar_->setObjectName("TopNavBar");
    pimpl_->top_bar_->setFixedHeight(65);
    auto* top_layout { new QHBoxLayout(pimpl_->top_bar_) };
    top_layout->setContentsMargins(40, 0, 40, 0);
    top_layout->setAlignment(Qt::AlignVCenter);

    top_layout->addStretch(); 

    auto* search_input { new QLineEdit };
    search_input->setPlaceholderText("Search containers, images, volumes...");
    search_input->setFixedWidth(350);
    search_input->setFixedHeight(36);
    search_input->setObjectName("SearchBox");
    search_input->addAction(QIcon(":/assets/icons/gemini-svg.svg"), QLineEdit::LeadingPosition); 

    pimpl_->settings_btn_ = new QPushButton("Settings");
    pimpl_->settings_btn_->setObjectName("SettingsBtn");
    pimpl_->settings_btn_->setCursor(Qt::PointingHandCursor);
    pimpl_->settings_btn_->setFixedHeight(36);
    pimpl_->settings_btn_->setCheckable(true); 

    top_layout->addWidget(search_input);
    top_layout->addStretch(); 
    top_layout->addWidget(pimpl_->settings_btn_);

    main_v_layout->addWidget(pimpl_->top_bar_);

    
    auto* content_wrapper { new QWidget };
    auto* layout { new QVBoxLayout(content_wrapper) };
    layout->setContentsMargins(40, 20, 40, 0);

    pimpl_->main_stack_ = new QStackedWidget;
    layout->addWidget(pimpl_->main_stack_);
    main_v_layout->addWidget(content_wrapper);
    
  
    pimpl_->auth_page_ = new QWidget;
    auto* auth_layout { new QVBoxLayout(pimpl_->auth_page_) };
    auth_layout->setAlignment(Qt::AlignCenter);
    auth_layout->setSpacing(10);
    
auto* logo_lbl = new QLabel;
    QPixmap logo_px(":/assets/icons/Quiver.svg"); 
    if (!logo_px.isNull()) {
        logo_lbl->setPixmap(logo_px.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo_lbl->setAlignment(Qt::AlignCenter);
        auth_layout->addWidget(logo_lbl);
    }

    auto* auth_title = new QLabel("Welcome to Quiver");
    auth_title->setStyleSheet("font-size: 24px; font-weight: bold; margin-bottom: 10px;");
    auth_title->setAlignment(Qt::AlignCenter);
    
    auto* auth_subtitle = new QLabel("Please sign in to manage your containers.");
    auth_subtitle->setStyleSheet("color: #A1A1AA; font-size: 14px; margin-bottom: 20px;");
    auth_subtitle->setAlignment(Qt::AlignCenter);
    
    auto* btn_browser_login = new QPushButton("Sign In with Browser");
    btn_browser_login->setObjectName("PrimaryButton");
    btn_browser_login->setFixedSize(250, 45);
    btn_browser_login->setCursor(Qt::PointingHandCursor);
    
    connect(btn_browser_login, &QPushButton::clicked, this, []() {
        AuthManager::get_instance().start_browser_login();
    });
    
    auth_layout->addWidget(auth_title, 0, Qt::AlignCenter);
    auth_layout->addWidget(auth_subtitle, 0, Qt::AlignCenter);
    auth_layout->addWidget(btn_browser_login, 0, Qt::AlignCenter);
    
   
    
    pimpl_->dashboard_page_ = new DashboardPage;
pimpl_->main_stack_->addWidget(pimpl_->dashboard_page_);


    pimpl_->containers_page_ = new ContainersPage;
    pimpl_->main_stack_->addWidget(pimpl_->containers_page_);
    
    pimpl_->details_page_ = new ContainerDetailsPage;
    pimpl_->main_stack_->addWidget(pimpl_->details_page_);

    connect(pimpl_->containers_page_, &ContainersPage::container_info_requested, this, [this](const QString& id) {
        pimpl_->details_page_->set_container_id(id);
        pimpl_->main_stack_->setCurrentWidget(pimpl_->details_page_);
    });
    
    connect(pimpl_->details_page_, &ContainerDetailsPage::back_requested, this, [this]() {
        pimpl_->main_stack_->setCurrentWidget(pimpl_->containers_page_);
    });


    pimpl_->images_page_ = new ImagesPage;
    pimpl_->main_stack_->addWidget(pimpl_->images_page_);


    pimpl_->volumes_page_ = new VolumesPage;
    pimpl_->main_stack_->addWidget(pimpl_->volumes_page_);


    pimpl_->ports_page_ = new PortsPage;
    pimpl_->main_stack_->addWidget(pimpl_->ports_page_);


    pimpl_->devices_page_ = new DevicesPage;
    pimpl_->main_stack_->addWidget(pimpl_->devices_page_);

    pimpl_->settings_page_ = new SettingsPage;
    pimpl_->main_stack_->addWidget(pimpl_->settings_page_);
    int settings_idx = pimpl_->main_stack_->count() - 1;

    
    connect(pimpl_->settings_btn_, &QPushButton::clicked, this, [this, settings_idx](){
        if (pimpl_->nav_group_) {
            pimpl_->nav_group_->setExclusive(false);
            for(auto* b : pimpl_->nav_group_->buttons()) b->setChecked(false);
            pimpl_->nav_group_->setExclusive(true);
        }
        switch_tab(settings_idx);
    });

    
   
    auto* light_card = pimpl_->settings_page_->findChild<QAbstractButton*>("ThemeLightCard");
    auto* dark_card  = pimpl_->settings_page_->findChild<QAbstractButton*>("ThemeDarkCard");
    
    if (light_card && dark_card) {
        connect(light_card, &QAbstractButton::clicked, this, [this]() {
            if (pimpl_->is_dark_mode_) toggle_theme();
        });
        connect(dark_card, &QAbstractButton::clicked, this, [this]() {
            if (!pimpl_->is_dark_mode_) toggle_theme();
        });
    }

     pimpl_->main_stack_->addWidget(pimpl_->auth_page_);
    pimpl_->main_stack_->setCurrentIndex(1); 
}





auto MainWindow::toggle_sidebar() -> void {
    bool will_expand { !pimpl_->is_sidebar_expanded_ };
    int start { will_expand ? 80 : 240 };
    int end   { will_expand ? 240 : 80 };

    auto* anim_group { new QParallelAnimationGroup(this) };
    auto* min_anim { new QPropertyAnimation(pimpl_->sidebar_, "minimumWidth") };
    min_anim->setDuration(300);
    min_anim->setStartValue(start);
    min_anim->setEndValue(end);
    min_anim->setEasingCurve(QEasingCurve::InOutQuad);
    auto* max_anim { new QPropertyAnimation(pimpl_->sidebar_, "maximumWidth") };
    max_anim->setDuration(300);
    max_anim->setStartValue(start);
    max_anim->setEndValue(end);
    max_anim->setEasingCurve(QEasingCurve::InOutQuad);
    anim_group->addAnimation(min_anim);
    anim_group->addAnimation(max_anim);
    anim_group->start(QAbstractAnimation::DeleteWhenStopped);

    QList<QPushButton*> btns { pimpl_->sidebar_->findChildren<QPushButton*>() };
    for (auto* btn : btns) {
        if (btn->property("navText").isValid()) {
            btn->setProperty("expanded", will_expand);
            btn->setText(will_expand ? btn->property("navText").toString() : "");
            btn->style()->unpolish(btn);
            btn->style()->polish(btn);
        }
    }
    pimpl_->is_sidebar_expanded_ = will_expand;
}

// auto MainWindow::toggle_theme() -> void {
//     QPixmap pixmap { this->grab() };
//     auto* overlay { new QLabel(this) };
//     overlay->setPixmap(pixmap);
//     overlay->setGeometry(this->rect());
//     overlay->show();
//     overlay->raise();

//     pimpl_->is_dark_mode_ = !pimpl_->is_dark_mode_;

//     QString theme_path { pimpl_->is_dark_mode_
//         ? ":/assets/style.qss" : ":/assets/light_style.qss" };
//     QFile file(theme_path);
//     if (file.open(QFile::ReadOnly)) {
//         qApp->setStyleSheet(file.readAll());
//         file.close();
//     }


//     update_sidebar_icons();

//     auto* eff { new QGraphicsOpacityEffect(overlay) };
//     overlay->setGraphicsEffect(eff);
//     auto* a { new QPropertyAnimation(eff, "opacity") };
//     a->setDuration(300);
//     a->setStartValue(1.0);
//     a->setEndValue(0.0);
//     connect(a, &QPropertyAnimation::finished, overlay, &QLabel::deleteLater);
//     a->start(QAbstractAnimation::DeleteWhenStopped);
// }

auto MainWindow::toggle_theme() -> void {
    QPixmap pixmap { this->grab() };
    auto* overlay { new QLabel(this) };
    overlay->setPixmap(pixmap);
    overlay->setGeometry(this->rect());
    overlay->show();
    overlay->raise();

    pimpl_->is_dark_mode_ = !pimpl_->is_dark_mode_;

    QString theme_path { pimpl_->is_dark_mode_
        ? ":/assets/style.qss" : ":/assets/light_style.qss" };
    QFile file(theme_path);
    if (file.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(file.readAll());
        file.close();
    }

    update_sidebar_icons();

    // --- ADD THIS MAGIC BLOCK ---
    // Dynamically re-color all charts across the entire application
    QColor text_color = pimpl_->is_dark_mode_ ? QColor("#A1A1AA") : QColor("#52525B");
    QColor grid_color = pimpl_->is_dark_mode_ ? QColor("#27272A") : QColor("#E4E4E7");

    // Find every QChartView currently loaded in the UI
    QList<QChartView*> chart_views = this->findChildren<QChartView*>();
    for (auto* view : chart_views) {
        QChart* chart = view->chart();
        if (chart) {
            chart->legend()->setLabelColor(text_color); // Update Legend Text
            for (auto* axis : chart->axes()) {
                axis->setLabelsColor(text_color);       // Update Axis Text
                axis->setGridLineColor(grid_color);     // Update Axis Gridlines
            }
        }
    }
    // ----------------------------

    auto* eff { new QGraphicsOpacityEffect(overlay) };
    overlay->setGraphicsEffect(eff);
    auto* a { new QPropertyAnimation(eff, "opacity") };
    a->setDuration(300);
    a->setStartValue(1.0);
    a->setEndValue(0.0);
    connect(a, &QPropertyAnimation::finished, overlay, &QLabel::deleteLater);
    a->start(QAbstractAnimation::DeleteWhenStopped);
}


auto MainWindow::switch_tab(int index) -> void {
    if (index >= 0 && index < pimpl_->main_stack_->count()) {
        pimpl_->main_stack_->setCurrentIndex(index);
        update_sidebar_icons();
    }
}

auto MainWindow::update_sidebar_icons() -> void {
    QColor normal_color { pimpl_->is_dark_mode_ ? QColor{"#A1A1AA"} : QColor{"#71717A"} };

    QColor active_color { QColor{"#F97316"} };

    QList<QPushButton*> btns { pimpl_->sidebar_->findChildren<QPushButton*>() };
    for (auto* btn : btns) {
        if (btn->objectName() == "ToggleBtn") continue;
        QString path { btn->property("iconPath").toString() };
        
        if (!path.isEmpty()) {
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
    if (pimpl_->containers_page_) {
        pimpl_->containers_page_->refresh();
    }
}

}
