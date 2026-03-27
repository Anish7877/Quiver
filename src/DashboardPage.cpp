#include "include/DashboardPage.h"
#include "include/Components.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>

namespace Quiver {

struct DashboardPage::Impl {
};


static auto create_image_card(const QString& name, const QString& tag, const QString& time) -> QFrame* {
    auto* card = new QFrame;
    card->setObjectName("DashItemCard");
    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(15, 10, 15, 10);

    auto* name_lbl = new QLabel(name);
    name_lbl->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    auto* tag_lbl = new QLabel(tag);
    tag_lbl->setStyleSheet("color: #F97316; font-size: 11px; font-weight: bold; background: rgba(249, 115, 22, 0.1); padding: 4px 8px; border-radius: 4px;");

    auto* time_lbl = new QLabel(time);
    time_lbl->setObjectName("DimText");

    auto* run_btn = new QPushButton("▶ Run");
    run_btn->setObjectName("PrimaryButton");
    run_btn->setCursor(Qt::PointingHandCursor);
    run_btn->setFixedSize(60, 28);

    layout->addWidget(name_lbl);
    layout->addWidget(tag_lbl);
    layout->addStretch();
    layout->addWidget(time_lbl);
    layout->addSpacing(10);
    layout->addWidget(run_btn);

    return card;
}


static auto create_event_item(const QString& type, const QString& message, const QString& theme) -> QFrame* {
    auto* row = new QFrame;
    row->setObjectName("DashEventRow");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(10, 8, 10, 8);

    auto* badge = new QLabel(type);
    badge->setProperty("statTheme", theme); 
    badge->setObjectName("EventBadge");

    auto* msg = new QLabel(message);
    msg->setStyleSheet("font-size: 13px;");

    layout->addWidget(badge);
    layout->addSpacing(10);
    layout->addWidget(msg);
    layout->addStretch();

    return row;
}

DashboardPage::DashboardPage(QWidget* parent)
    : QWidget(parent), pimpl_{std::make_unique<Impl>()}
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(24);


    auto* title = new QLabel("Dashboard");
    title->setObjectName("PageTitle");
    root->addWidget(title);

  
    auto* stats_row = new QHBoxLayout;
    stats_row->setSpacing(20);
    stats_row->addWidget(new StatCard("TOTAL CONTAINERS", "14", "orange"));
    stats_row->addWidget(new StatCard("RUNNING", "8", "green"));
    stats_row->addWidget(new StatCard("FAILED / STOPPED", "2", "red"));
    stats_row->addWidget(new StatCard("ACTIVE VOLUMES", "5", "white"));
    root->addLayout(stats_row);

    
    auto* split_layout = new QHBoxLayout;
    split_layout->setSpacing(24);

  
    auto* left_pane = new QFrame;
    left_pane->setObjectName("DashPanel");
    auto* left_layout = new QVBoxLayout(left_pane);
    left_layout->setContentsMargins(20, 20, 20, 20);
    left_layout->setSpacing(10);
    
    auto* left_title = new QLabel("Recent Container (Quick Launch)");
    left_title->setObjectName("DashPanelTitle");
    left_layout->addWidget(left_title);
    left_layout->addSpacing(5);

    left_layout->addWidget(create_image_card("ubuntu", "22.04", "Pulled 2 hours ago"));
    left_layout->addWidget(create_image_card("nginx", "latest", "Pulled yesterday"));
    left_layout->addWidget(create_image_card("postgres", "14-alpine", "Pulled 3 days ago"));
    left_layout->addWidget(create_image_card("redis", "6.2", "Pulled 1 week ago"));
    left_layout->addStretch();

   
    auto* right_pane = new QFrame;
    right_pane->setObjectName("DashPanel");
    auto* right_layout = new QVBoxLayout(right_pane);
    right_layout->setContentsMargins(20, 20, 20, 20);
    right_layout->setSpacing(10);

    auto* right_title = new QLabel("System Activity");
    right_title->setObjectName("DashPanelTitle");
    right_layout->addWidget(right_title);
    right_layout->addSpacing(5);

    right_layout->addWidget(create_event_item("CRASH", "Container 'db-worker' exited with code 137 (OOM)", "red"));
    right_layout->addWidget(create_event_item("PULLED", "Successfully fetched 'ubuntu:22.04'", "green"));
    right_layout->addWidget(create_event_item("STARTED", "Container 'nginx-proxy' is now running", "green"));
    right_layout->addWidget(create_event_item("MOUNTED", "Volume 'pg_data' attached to 'postgres-main'", "white"));
    right_layout->addWidget(create_event_item("WARNING", "Docker daemon CPU usage exceeded 85%", "orange"));
    right_layout->addStretch();

    
    split_layout->addWidget(left_pane, 6);  
    split_layout->addWidget(right_pane, 4); 
    root->addLayout(split_layout, 1); 
}

DashboardPage::~DashboardPage() = default;

} 
