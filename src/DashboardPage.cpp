#include "include/DashboardPage.h"
#include "include/Components.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QLinearGradient>
#include <QGraphicsLayout>
#include <QtCharts/QHorizontalBarSeries>

namespace Quiver {

struct DashboardPage::Impl {
};

static auto create_cpu_cores_panel(const QString& title) -> QFrame* {
    auto* panel = new QFrame;
    panel->setObjectName("DashPanel");
    panel->setMinimumHeight(240);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 10);

    auto* titleLbl = new QLabel(title);
    titleLbl->setObjectName("DashPanelTitle");
    layout->addWidget(titleLbl);

    auto* series = new QHorizontalBarSeries();
    auto* set = new QBarSet("Core Load");
    *set << 35 << 82 << 45 << 60; 
    set->setBrush(QColor("#F97316")); 
    set->setBorderColor(QColor("#F97316"));
    series->append(set);
    series->setBarWidth(0.5);

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->legend()->hide();
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(0, 10, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);

    auto* axisY = new QBarCategoryAxis();
    axisY->append({"Core 4", "Core 3", "Core 2", "Core 1"});
    axisY->setLabelsColor(QColor("#A1A1AA"));
    axisY->setGridLineVisible(false);
    axisY->setLineVisible(false);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    auto* axisX = new QValueAxis();
    axisX->setRange(0, 100);
    axisX->setLabelsColor(QColor("#A1A1AA"));
    axisX->setGridLineColor(QColor("#27272A"));
    axisX->setLineVisible(false);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setStyleSheet("background: transparent;");
    layout->addWidget(chartView);
    return panel;
}

static auto create_donut_chart_panel(const QString& title) -> QFrame* {
    auto* panel = new QFrame;
    panel->setObjectName("DashPanel");
    panel->setMinimumHeight(240);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 10);

    auto* titleLbl = new QLabel(title);
    titleLbl->setObjectName("DashPanelTitle");
    layout->addWidget(titleLbl);

    auto* series = new QPieSeries();
    series->setHoleSize(0.45); 


    auto* s1 = series->append("App (4GB)", 4.0); s1->setBrush(QColor("#4ade80")); s1->setBorderColor(QColor("#4ade80"));
    auto* s2 = series->append("Cache (1.5GB)", 1.5); s2->setBrush(QColor("#3b82f6")); s2->setBorderColor(QColor("#3b82f6"));
    auto* s3 = series->append("Free (2.5GB)", 2.5); s3->setBrush(QColor("#27272A")); s3->setBorderColor(QColor("#27272A"));

    auto* chart = new QChart();
    chart->addSeries(series);
    

    chart->legend()->show();
    chart->legend()->setAlignment(Qt::AlignRight);
    chart->legend()->setLabelColor(QColor("#A1A1AA"));
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);

    auto* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setStyleSheet("background: transparent;");
    layout->addWidget(chartView);
    return panel;
}


static auto create_multi_bar_chart_panel(const QString& title, const QStringList& ports, const QList<qreal>& traffic, const QList<QColor>& colors) -> QFrame* {
    auto* panel = new QFrame;
    panel->setObjectName("DashPanel");
    panel->setMinimumHeight(240);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 10);

    auto* titleLbl = new QLabel(title);
    titleLbl->setObjectName("DashPanelTitle");
    layout->addWidget(titleLbl);

    auto* series = new QBarSeries();
    
   
    for (int i = 0; i < ports.size(); ++i) {
        auto* set = new QBarSet(ports[i]);
        *set << traffic[i];
        set->setBrush(colors[i]);
        set->setBorderColor(colors[i]);
        series->append(set);
    }
    series->setBarWidth(0.6);

    auto* chart = new QChart();
    chart->addSeries(series);
    

    chart->legend()->show();
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor("#A1A1AA"));
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(0, 10, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);


    auto* axisX = new QBarCategoryAxis();
    axisX->append(QStringList{"Connections"});
    axisX->setLabelsColor(QColor("#A1A1AA"));
    axisX->setGridLineVisible(false);
    axisX->setLineVisible(false);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis();
    axisY->setLabelsColor(QColor("#A1A1AA"));
    axisY->setGridLineColor(QColor("#27272A"));
    axisY->setLineVisible(false);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    auto* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setStyleSheet("background: transparent;");
    layout->addWidget(chartView);
    return panel;
}


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
    run_btn->setFixedSize(75, 28);

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


    auto* charts_row = new QHBoxLayout;
    charts_row->setSpacing(24);
    
    charts_row->addWidget(create_cpu_cores_panel("CPU Usage by Core (%)"));

    charts_row->addWidget(create_donut_chart_panel("Memory Allocation"));

    QStringList portLabels = {"Port 80", "Port 443", "Port 5432", "Port 6379"};
    QList<qreal> portTraffic = {120, 250, 45, 80};
    QList<QColor> portColors = {QColor("#F97316"), QColor("#3b82f6"), QColor("#4ade80"), QColor("#fb7185")};
    charts_row->addWidget(create_multi_bar_chart_panel("Active Port Connections", portLabels, portTraffic, portColors));
    
    root->addLayout(charts_row);


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