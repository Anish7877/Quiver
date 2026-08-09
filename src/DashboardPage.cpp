#include "include/DashboardPage.h"
#include "include/Components.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QHorizontalBarSeries>
#include <QEasingCurve>
#include <QGraphicsLayout>

#include <QTimer>
#include <QPlainTextEdit>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QProcess>
#include <QScrollBar>
#include <QRegularExpression>
#include <QFileSystemWatcher>
#include "include/Backend.h"

namespace Quiver {

struct DashboardPage::Impl {
    QScrollArea* scroll_area_{};
    QWidget* scroll_content_{};
    QTimer* stats_timer_{};
    QPlainTextEdit* log_viewer_{};
    QChart* cpu_chart_{};
    QChart* mem_chart_{};
    QBarSeries* cpu_series_{};
    QBarSeries* mem_series_{};
    QBarCategoryAxis* cpu_axisX_{};
    QBarCategoryAxis* mem_axisX_{};
    uint64_t last_log_size_{};
    QFileSystemWatcher* log_watcher_{};
    QProcess* stats_process_{};
};
static auto create_cpu_cores_panel(const QString& title) -> QFrame* {
    auto* panel = new QFrame;
    panel->setObjectName("DashPanel");
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 10);

    auto* titleLbl = new QLabel(title);
    titleLbl->setObjectName("DashPanelTitle");
    layout->addWidget(titleLbl);

    auto* series = new QHorizontalBarSeries();
    auto* set = new QBarSet("Core Load");
    *set << 35 << 82 << 45 << 60; 
    set->setBrush(QColor("#F97316")); 
    set->setPen(Qt::NoPen);
    
    series->append(set);
    series->setBarWidth(0.4);

    auto* chart = new QChart();
    
  
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setAnimationEasingCurve(QEasingCurve::OutQuart);
    chart->setAnimationDuration(1200); 
   

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
    chartView->setMinimumHeight(160); // Allows scaling
    layout->addWidget(chartView);
    return panel;
}

static auto create_donut_chart_panel(const QString& title) -> QFrame* {
    auto* panel = new QFrame;
    panel->setObjectName("DashPanel");
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(20, 15, 20, 10);

    auto* titleLbl = new QLabel(title);
    titleLbl->setObjectName("DashPanelTitle");
    layout->addWidget(titleLbl);

    auto* series = new QPieSeries();
    series->setHoleSize(0.50); 


    
    auto* s1 = series->append("App", 4.0); 
    s1->setBrush(QColor("#F97316")); 
    s1->setPen(Qt::NoPen);

    
    auto* s2 = series->append("Cache", 1.5); 
    s2->setBrush(QColor("#71717A")); // Zinc-500
    s2->setPen(Qt::NoPen);


    auto* s3 = series->append("Free", 2.5); 
    s3->setBrush(QColor(161, 161, 170, 40)); 
    s3->setPen(Qt::NoPen);

    auto* chart = new QChart();
    

    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setAnimationEasingCurve(QEasingCurve::OutCubic);
    chart->setAnimationDuration(1500);
    
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
    chartView->setMinimumHeight(160);
    layout->addWidget(chartView);
    return panel;
}

static auto create_multi_bar_chart_panel(const QString& title, const QStringList& ports, const QList<qreal>& traffic, const QList<QColor>& colors) -> QFrame* {
    auto* panel = new QFrame;
    panel->setObjectName("DashPanel");
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
        set->setPen(Qt::NoPen);
        series->append(set);    
    }
    series->setBarWidth(0.5);

    auto* chart = new QChart();
    
   
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->setAnimationEasingCurve(QEasingCurve::OutElastic); 
    chart->setAnimationDuration(1800);
   

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
    chartView->setMinimumHeight(160);
    layout->addWidget(chartView);
    return panel;
}



DashboardPage::DashboardPage(QWidget* parent)
    : QWidget(parent), pimpl_{std::make_unique<Impl>()}
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    
    pimpl_->scroll_area_ = new QScrollArea(this);
    pimpl_->scroll_area_->setWidgetResizable(true);
    pimpl_->scroll_area_->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    pimpl_->scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    pimpl_->scroll_content_ = new QWidget();
    pimpl_->scroll_content_->setStyleSheet("background: transparent;");
    auto* scroll_layout = new QVBoxLayout(pimpl_->scroll_content_);
    scroll_layout->setContentsMargins(0, 0, 0, 20); 
    scroll_layout->setSpacing(24);

    auto* title = new QLabel("Dashboard");
    title->setObjectName("PageTitle");
    scroll_layout->addWidget(title);

    auto* stats_row = new QHBoxLayout;
    stats_row->setSpacing(20);
    stats_row->addWidget(new StatCard("TOTAL CONTAINERS", "14", "orange"));
    stats_row->addWidget(new StatCard("RUNNING", "8", "green"));
    stats_row->addWidget(new StatCard("FAILED / STOPPED", "2", "red"));
    stats_row->addWidget(new StatCard("ACTIVE VOLUMES", "5", "white"));
    scroll_layout->addLayout(stats_row);



    auto* split_layout = new QHBoxLayout;
    split_layout->setSpacing(24);

    auto* left_pane = new QFrame;
    left_pane->setObjectName("DashPanel");
    left_pane->setStyleSheet("QFrame#DashPanel { background: transparent; border: none; }");
    auto* left_layout = new QVBoxLayout(left_pane);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(24);
    
    // CPU Chart Panel
    auto* cpu_panel = new QFrame;
    cpu_panel->setObjectName("DashPanel");
    auto* cpu_layout = new QVBoxLayout(cpu_panel);
    cpu_layout->setContentsMargins(20, 15, 20, 10);
    auto* cpu_title = new QLabel("CPU Utilization (%)");
    cpu_title->setObjectName("DashPanelTitle");
    cpu_layout->addWidget(cpu_title);

    pimpl_->cpu_chart_ = new QChart();
    pimpl_->cpu_chart_->legend()->hide();
    pimpl_->cpu_series_ = new QBarSeries();
    pimpl_->cpu_chart_->addSeries(pimpl_->cpu_series_);
    pimpl_->cpu_axisX_ = new QBarCategoryAxis();
    pimpl_->cpu_axisX_->setLabelsColor(QColor("#A1A1AA"));
    pimpl_->cpu_axisX_->setGridLineVisible(false);
    pimpl_->cpu_chart_->addAxis(pimpl_->cpu_axisX_, Qt::AlignBottom);
    pimpl_->cpu_series_->attachAxis(pimpl_->cpu_axisX_);
    
    auto* cpu_axisY = new QValueAxis();
    cpu_axisY->setRange(0, 100);
    cpu_axisY->setLabelsColor(QColor("#A1A1AA"));
    cpu_axisY->setGridLineColor(QColor("#27272A"));
    pimpl_->cpu_chart_->addAxis(cpu_axisY, Qt::AlignLeft);
    pimpl_->cpu_series_->attachAxis(cpu_axisY);
    
    pimpl_->cpu_chart_->setBackgroundVisible(false);
    pimpl_->cpu_chart_->setMargins(QMargins(0, 10, 0, 0));
    pimpl_->cpu_chart_->layout()->setContentsMargins(0, 0, 0, 0);
    
    auto* cpu_view = new QChartView(pimpl_->cpu_chart_);
    cpu_view->setRenderHint(QPainter::Antialiasing);
    cpu_view->setStyleSheet("background: transparent;");
    cpu_view->setMinimumHeight(250);
    cpu_layout->addWidget(cpu_view);
    left_layout->addWidget(cpu_panel);

    // Memory Chart Panel
    auto* mem_panel = new QFrame;
    mem_panel->setObjectName("DashPanel");
    auto* mem_layout = new QVBoxLayout(mem_panel);
    mem_layout->setContentsMargins(20, 15, 20, 10);
    auto* mem_title = new QLabel("Memory Allocation (MB)");
    mem_title->setObjectName("DashPanelTitle");
    mem_layout->addWidget(mem_title);

    pimpl_->mem_chart_ = new QChart();
    pimpl_->mem_chart_->legend()->hide();
    pimpl_->mem_series_ = new QBarSeries();
    pimpl_->mem_chart_->addSeries(pimpl_->mem_series_);
    pimpl_->mem_axisX_ = new QBarCategoryAxis();
    pimpl_->mem_axisX_->setLabelsColor(QColor("#A1A1AA"));
    pimpl_->mem_axisX_->setGridLineVisible(false);
    pimpl_->mem_chart_->addAxis(pimpl_->mem_axisX_, Qt::AlignBottom);
    pimpl_->mem_series_->attachAxis(pimpl_->mem_axisX_);
    
    auto* mem_axisY = new QValueAxis();
    mem_axisY->setRange(0, 1024);
    mem_axisY->setLabelsColor(QColor("#A1A1AA"));
    mem_axisY->setGridLineColor(QColor("#27272A"));
    pimpl_->mem_chart_->addAxis(mem_axisY, Qt::AlignLeft);
    pimpl_->mem_series_->attachAxis(mem_axisY);
    
    pimpl_->mem_chart_->setBackgroundVisible(false);
    pimpl_->mem_chart_->setMargins(QMargins(0, 10, 0, 0));
    pimpl_->mem_chart_->layout()->setContentsMargins(0, 0, 0, 0);
    
    auto* mem_view = new QChartView(pimpl_->mem_chart_);
    mem_view->setRenderHint(QPainter::Antialiasing);
    mem_view->setStyleSheet("background: transparent;");
    mem_view->setMinimumHeight(250);
    mem_layout->addWidget(mem_view);
    left_layout->addWidget(mem_panel);

    // Right Pane (Logs)
    auto* right_pane = new QFrame;
    right_pane->setObjectName("DashPanel");
    auto* right_layout = new QVBoxLayout(right_pane);
    right_layout->setContentsMargins(20, 20, 20, 20);
    right_layout->setSpacing(10);
    
    auto* logs_title = new QLabel("Logs");
    logs_title->setObjectName("DashPanelTitle");
    right_layout->addWidget(logs_title);
    
    pimpl_->log_viewer_ = new QPlainTextEdit;
    pimpl_->log_viewer_->setReadOnly(true);
    pimpl_->log_viewer_->setStyleSheet("QPlainTextEdit { background-color: #0d1117; color: #c9d1d9; font-family: monospace; border: 1px solid #30363d; border-radius: 4px; padding: 10px; }");
    right_layout->addWidget(pimpl_->log_viewer_);

    split_layout->addWidget(left_pane, 5);  
    split_layout->addWidget(right_pane, 5); 
    scroll_layout->addLayout(split_layout); 

    pimpl_->scroll_area_->setWidget(pimpl_->scroll_content_);
    root->addWidget(pimpl_->scroll_area_);

    // Setup Logs File Watcher
    QString log_path = QDir::homePath() + "/.quiver/logs/container.log";
    pimpl_->log_watcher_ = new QFileSystemWatcher(this);
    
    // Create directory and dummy file if it doesn't exist
    QDir().mkpath(QDir::homePath() + "/.quiver/logs");
    QFile dummy(log_path);
    if (!dummy.exists()) {
        dummy.open(QIODevice::WriteOnly);
        dummy.close();
    }
    
    pimpl_->log_watcher_->addPath(log_path);
    
    auto read_logs = [this, log_path]() {
        QFile file(log_path);
        if (file.open(QIODevice::ReadOnly)) {
            qint64 currentSize = file.size();
            if (currentSize > (qint64)pimpl_->last_log_size_) {
                if (file.seek(pimpl_->last_log_size_)) {
                    QByteArray new_logs = file.readAll();
                    if (!new_logs.isEmpty()) {
                        QTextCursor cursor = pimpl_->log_viewer_->textCursor();
                        cursor.movePosition(QTextCursor::End);
                        pimpl_->log_viewer_->setTextCursor(cursor);
                        pimpl_->log_viewer_->insertPlainText(QString::fromUtf8(new_logs));
                        
                        QScrollBar *bar = pimpl_->log_viewer_->verticalScrollBar();
                        bar->setValue(bar->maximum());
                    }
                    pimpl_->last_log_size_ = currentSize;
                }
            } else if (currentSize < (qint64)pimpl_->last_log_size_) {
                pimpl_->last_log_size_ = 0;
                pimpl_->log_viewer_->clear();
            }
            file.close();
        }
    };
    
    connect(pimpl_->log_watcher_, &QFileSystemWatcher::fileChanged, this, read_logs);
    read_logs(); // Initial read

    // Setup Stats Polling Process
    pimpl_->stats_process_ = new QProcess(this);
    connect(pimpl_->stats_process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitStatus != QProcess::NormalExit || exitCode != 0) return;
        
        QString output = pimpl_->stats_process_->readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        if (lines.isEmpty()) return;

        QBarSet* cpu_set = new QBarSet("CPU");
        QBarSet* mem_set = new QBarSet("MEM");
        cpu_set->setBrush(QColor("#58a6ff")); // Blue
        cpu_set->setPen(Qt::NoPen);
        mem_set->setBrush(QColor("#2ea043")); // Green
        mem_set->setPen(Qt::NoPen);

        QStringList categories;
        double max_mem = 100;
        double max_cpu = 100;

        for (int i = 1; i < lines.size(); ++i) { // skip header
            QString line = lines[i].trimmed();
            if (line.isEmpty()) continue;
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                QString id = parts[0].left(8);
                QString cpu_str = parts[1]; 
                cpu_str.remove("%");
                QString mem_str = parts[2]; 
                mem_str.remove("MB");
                
                double cpu = cpu_str.toDouble();
                double mem = mem_str.toDouble();
                
                *cpu_set << cpu;
                *mem_set << mem;
                categories << id;
                if (mem > max_mem) max_mem = mem;
                if (cpu > max_cpu) max_cpu = cpu;
            }
        }

        if (!categories.isEmpty()) {
            pimpl_->cpu_series_->clear();
            pimpl_->cpu_series_->append(cpu_set);
            pimpl_->cpu_axisX_->clear();
            pimpl_->cpu_axisX_->append(categories);
            if (auto axis = qobject_cast<QValueAxis*>(pimpl_->cpu_chart_->axes(Qt::Vertical).first())) {
                axis->setRange(0, max_cpu * 1.2); 
            }

            pimpl_->mem_series_->clear();
            pimpl_->mem_series_->append(mem_set);
            pimpl_->mem_axisX_->clear();
            pimpl_->mem_axisX_->append(categories);
            if (auto axis = qobject_cast<QValueAxis*>(pimpl_->mem_chart_->axes(Qt::Vertical).first())) {
                axis->setRange(0, max_mem * 1.2); 
            }
        } else {
            delete cpu_set;
            delete mem_set;
        }
    });

    // Timer logic to trigger stats process
    pimpl_->stats_timer_ = new QTimer(this);
    connect(pimpl_->stats_timer_, &QTimer::timeout, this, [this]() {
        if (pimpl_->stats_process_->state() == QProcess::NotRunning) {
            pimpl_->stats_process_->start(Backend::get_instance().get_cli_path(), {"stats"});
        }
    });
    pimpl_->stats_timer_->start(2000);
}

DashboardPage::~DashboardPage() = default;

}