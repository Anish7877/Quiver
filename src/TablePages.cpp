#include "include/TablePages.h"
#include "include/Components.h"
#include "include/Backend.h"
#include <QRandomGenerator>
#include <initializer_list>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QFrame>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QWidget>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QSet>
#include <QSizePolicy>
#include <QButtonGroup>
#include <QToolButton>
#include <QLayout>
#include <QFileDialog>
#include <QTimer>
#include <QProcess>
#include <QFileInfo>
#include "include/Components.h"
#include "include/AuthManager.h"
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include<QSettings>
#include <QApplication>
#include <QClipboard>

namespace Quiver {
namespace {

QIcon createActionIcon(const QString& type, const QColor& color) {
    QIcon icon;
    
    auto drawPixmap = [&type](const QColor& drawColor) {
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        
        if (type == "play") {
            p.setBrush(drawColor);
            p.setPen(Qt::NoPen);
            QPolygonF poly;
            poly << QPointF(4, 2) << QPointF(14, 8) << QPointF(4, 14);
            p.drawPolygon(poly);
        } else if (type == "stop") {
            p.setBrush(drawColor);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(3, 3, 10, 10, 2, 2);
        } else if (type == "pause") {
            p.setBrush(drawColor);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(3, 3, 4, 10, 1, 1);
            p.drawRoundedRect(9, 3, 4, 10, 1, 1);
        } else if (type == "delete") {
            p.setPen(QPen(drawColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawLine(3, 4, 13, 4);
            p.drawLine(6, 4, 6, 2); p.drawLine(6, 2, 10, 2); p.drawLine(10, 2, 10, 4);
            p.drawRoundedRect(4, 4, 8, 10, 1, 1);
            p.drawLine(6, 6, 6, 11);
            p.drawLine(10, 6, 10, 11);
        } else if (type == "prune") {
            p.setPen(QPen(drawColor, 1.5, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawLine(4, 12, 12, 4); 
            p.drawEllipse(3, 3, 10, 10);
        } else if (type == "attach") {
            p.setPen(QPen(drawColor, 1.5, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            p.drawArc(2, 5, 6, 6, 90 * 16, 180 * 16);
            p.drawArc(8, 5, 6, 6, -90 * 16, 180 * 16);
            p.drawLine(6, 8, 10, 8);
        } else if (type == "copy") {
            p.setPen(QPen(drawColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(6, 6, 7, 8, 1, 1);
            p.drawPolyline(QPolygonF() << QPointF(9, 3) << QPointF(3, 3) << QPointF(3, 11));
        } else if (type == "info") {
            p.setPen(QPen(drawColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(2, 2, 12, 12);
            p.drawLine(8, 5, 8, 6);
            p.drawLine(8, 8, 8, 11);
        } else if (type == "refresh") {
            p.setPen(QPen(drawColor, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawArc(3, 3, 10, 10, 45 * 16, 270 * 16);
            p.drawPolyline(QPolygonF() << QPointF(13, 7) << QPointF(13, 3) << QPointF(9, 3));
        }
        return pixmap;
    };

    icon.addPixmap(drawPixmap(color), QIcon::Normal, QIcon::On);
    icon.addPixmap(drawPixmap(color), QIcon::Normal, QIcon::Off);
    
    QString theme = QSettings("Quiver", "Settings").value("theme", "dark").toString();
    QColor hoverColor = (theme == "light") ? QColor("#09090B") : QColor("#FAFAFA");
    
    icon.addPixmap(drawPixmap(hoverColor), QIcon::Active, QIcon::On);
    icon.addPixmap(drawPixmap(hoverColor), QIcon::Active, QIcon::Off);
    
    return icon;
}

class HoverIconButton : public QPushButton {
public:
    HoverIconButton(const QString& text, const QString& iconType, const QString& color, QWidget* parent = nullptr)
        : QPushButton(text, parent), iconType_(iconType), baseColor_(QColor(color)) {
        
        QString theme = QSettings("Quiver", "Settings").value("theme", "dark").toString();
        hoverColor_ = (theme == "light") ? QColor("#09090B") : QColor("#FAFAFA");
        
        normalIcon_ = createActionIcon(iconType_, baseColor_);
        hoverIcon_ = createActionIcon(iconType_, hoverColor_);
        
        setIcon(normalIcon_);
    }

protected:
    void enterEvent(QEnterEvent* event) override {
        setIcon(hoverIcon_);
        QPushButton::enterEvent(event);
    }
    
    void leaveEvent(QEvent* event) override {
        setIcon(normalIcon_);
        QPushButton::leaveEvent(event);
    }

private:
    QString iconType_;
    QColor baseColor_;
    QColor hoverColor_;
    QIcon normalIcon_;
    QIcon hoverIcon_;
};

auto make_item(const QString& text, bool bright = false) -> QTableWidgetItem* {
    auto* item { new QTableWidgetItem(text) };
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);

    return item;
}

auto make_status_badge(const QString& status) -> QWidget* {
    QString status_lower = status.toLower();
    QString bg, fg;
    
    if (status_lower == "running" || status_lower == "active" || status_lower == "open" || status_lower == "mounted") {
        bg = "rgba(74,222,128,0.12)";
        fg = "#4ade80";
    } else if (status_lower == "paused") {
        bg = "rgba(234,179,8,0.12)";
        fg = "#eab308";
    } else { // stopped, failed, unused
        bg = "rgba(251,113,133,0.12)";
        fg = "#fb7185";
    }

    auto* cell { new QWidget };
    auto* h { new QHBoxLayout(cell) };
    h->setContentsMargins(12, 0, 0, 0);
    h->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* badge { new QFrame };
    badge->setFixedHeight(26);
    badge->setMinimumWidth(85);
    badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    badge->setStyleSheet(QString(
                             "QFrame {"
                             "  background: %1;"
                             "  border: 1px solid %2;"
                             "  border-radius: 13px;"
                             "}"
                             ).arg(bg, fg));

    auto* bl { new QHBoxLayout(badge) };
    bl->setContentsMargins(10, 0, 10, 0);
    bl->setSpacing(6);

    auto* dot { new QLabel };
    dot->setFixedSize(6, 6);
    dot->setStyleSheet(QString(
                           "background: %1; border-radius: 3px; border: none;"
                           ).arg(fg));

    auto* lbl { new QLabel(status.toUpper()) };
    lbl->setStyleSheet(QString(
                           "color: %1; font-size: 10px; font-weight: 700;"
                           "letter-spacing: 0.5px; background: transparent; border: none;"
                           ).arg(fg));

    bl->addWidget(dot);
    bl->addWidget(lbl);
    h->addWidget(badge);
    return cell;
}

}


struct TablePage::Impl {
    QTableWidget* table_   {};
    QPushButton*  add_btn_ {};
    QHBoxLayout*  header_layout_ {};
    bool          has_actions_ {true};
};

TablePage::TablePage(const QString& title,
                     const QStringList& columns,
                     QWidget* parent,
                     bool has_actions)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{
    pimpl_->has_actions_ = has_actions;
    auto* layout { new QVBoxLayout(this) };
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    pimpl_->header_layout_ = new QHBoxLayout;
    auto* title_lbl { new QLabel(title) };
    title_lbl->setObjectName("PageTitle");
    title_lbl->setContentsMargins(0, 0, 0, 16);
    pimpl_->header_layout_->addWidget(title_lbl);
    
    layout->addLayout(pimpl_->header_layout_);


    QStringList all_cols { columns };
    if (has_actions) {
        all_cols << "ACTIONS";
    }

    pimpl_->table_ = new QTableWidget;
    pimpl_->table_->setObjectName("MainTable");
    pimpl_->table_->setColumnCount(all_cols.size());
    pimpl_->table_->setHorizontalHeaderLabels(all_cols);
    pimpl_->table_->verticalHeader()->setVisible(false);
    pimpl_->table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    pimpl_->table_->setSelectionMode(QAbstractItemView::SingleSelection);
    pimpl_->table_->setShowGrid(false);
    pimpl_->table_->setFocusPolicy(Qt::NoFocus);
    pimpl_->table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pimpl_->table_->setAlternatingRowColors(false);
    pimpl_->table_->verticalHeader()->setDefaultSectionSize(52);
    pimpl_->table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pimpl_->table_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pimpl_->table_->horizontalHeader()->setHighlightSections(false);
    pimpl_->table_->horizontalHeader()->setMinimumSectionSize(80);
    pimpl_->table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    int action_col { static_cast<int>(all_cols.size()) - 1 };
    pimpl_->table_->horizontalHeader()->setSectionResizeMode(
        action_col, QHeaderView::Fixed);

    pimpl_->table_->setColumnWidth(action_col, 280);
    pimpl_->table_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    
    pimpl_->table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    layout->addWidget(pimpl_->table_); 
    layout->addStretch(1);             


    auto* fab_row { new QHBoxLayout };
    fab_row->setContentsMargins(0, 16, 0, 0);
    pimpl_->add_btn_ = new QPushButton(); 
    pimpl_->add_btn_->setObjectName("FabButton");
    
    
    pimpl_->add_btn_->setCursor(Qt::PointingHandCursor);
    pimpl_->add_btn_->setFixedSize(100, 100); 

    connect(pimpl_->add_btn_, &QPushButton::clicked,
            this, &TablePage::add_clicked);
    fab_row->addStretch();
    fab_row->addWidget(pimpl_->add_btn_);
    layout->addLayout(fab_row);
}

TablePage::~TablePage() = default;

auto TablePage::add_row(const QStringList& row_data,
                        const QString&     action_label,
                        const QString&     action_obj_name) -> void
{
    constexpr int status_col { 1 };
    int row { pimpl_->table_->rowCount() };
    pimpl_->table_->insertRow(row);

    for (int col {}; col < row_data.size(); ++col) {
        if (col == status_col) {
            pimpl_->table_->setCellWidget(row, col, make_status_badge(row_data[col]));
        } else {
            pimpl_->table_->setItem(row, col, make_item(row_data[col], col == 0));
        }
    }


    int act_col { pimpl_->table_->columnCount() - 1 };
    auto* cell { new QWidget };
    auto* h { new QHBoxLayout(cell) };
    h->setContentsMargins(10, 0, 10, 0);
    h->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto* btn { new QPushButton(action_label) };
    btn->setObjectName(action_obj_name);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(82, 28);
    h->addWidget(btn);
    pimpl_->table_->setCellWidget(row, act_col, cell);

    connect(btn, &QPushButton::clicked, this, [this, btn]() {
        int cols { pimpl_->table_->columnCount() };
        for (int r {}; r < pimpl_->table_->rowCount(); ++r) {
            auto* w { pimpl_->table_->cellWidget(r, cols - 1) };
            if (w && w->findChild<QPushButton*>() == btn) {
                pimpl_->table_->removeRow(r);
                pimpl_->table_->updateGeometry(); 
                break;
            }
        }
    });

    pimpl_->table_->updateGeometry(); 
}

auto TablePage::table() -> QTableWidget* { return pimpl_->table_; }
auto TablePage::header_layout() -> QHBoxLayout* { return pimpl_->header_layout_; }


static auto make_stat_row(
    const QString& t1, const QString& v1,
    const QString& t2, const QString& v2, const QString& c2,
    const QString& t3, const QString& v3, const QString& c3,
    QVBoxLayout* root) -> void
{
    auto* row { new QHBoxLayout };
    row->setSpacing(20);
    row->setContentsMargins(0, 0, 0, 24);

    
    
    row->addWidget(new Quiver::StatCard(t1, v1, "#F97316"));
    row->addWidget(new Quiver::StatCard(t2, v2, c2));
    row->addWidget(new Quiver::StatCard(t3, v3, c3));

    root->addLayout(row);
}

struct ContainersPage::Impl {
    TablePage* page_ {};
    QWidget* overlay_ {};
    QLabel* loading_text_ {};
    QWidget* bulk_widget_ {};
    Quiver::StatCard* stat_total_ {};
    Quiver::StatCard* stat_running_ {};
    Quiver::StatCard* stat_stopped_ {};
    Quiver::StatCard* stat_paused_ {};
    
    QList<QPair<QString, QString>> get_selected_containers() {
        QList<QPair<QString, QString>> res;
        for (int r = 0; r < page_->table()->rowCount(); ++r) {
            auto* cell = page_->table()->cellWidget(r, 0);
            if (!cell) continue;
            auto* cb = cell->findChild<QPushButton*>("SelectCheckbox");
            if (cb && cb->isChecked()) {
                auto* item = page_->table()->item(r, 1);
                if (item) res.append({item->text(), item->data(Qt::UserRole).toString()});
            }
        }
        return res;
    }
    
    void update_bulk_actions_visibility() {
        if (bulk_widget_) {
            bulk_widget_->setVisible(!get_selected_containers().isEmpty());
        }
    }
};

ContainersPage::ContainersPage(QWidget* parent)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{
    auto* root { new QVBoxLayout(this) };
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* stats_row { new QHBoxLayout };
    stats_row->setSpacing(20);
    stats_row->setContentsMargins(0, 0, 0, 24);
    pimpl_->stat_total_ = new Quiver::StatCard("TOTAL", "0", "#ffffff");
    pimpl_->stat_running_ = new Quiver::StatCard("RUNNING", "0", "#4ade80");
    pimpl_->stat_stopped_ = new Quiver::StatCard("STOPPED", "0", "#fb7185");
    pimpl_->stat_paused_ = new Quiver::StatCard("PAUSED", "0", "#eab308");
    stats_row->addWidget(pimpl_->stat_total_);
    stats_row->addWidget(pimpl_->stat_running_);
    stats_row->addWidget(pimpl_->stat_stopped_);
    stats_row->addWidget(pimpl_->stat_paused_);
    root->addLayout(stats_row);

    pimpl_->page_ = new TablePage(
        "Containers",
        { "SELECT", "ID", "NAME", "IMAGE", "STATUS" },
        this);
    
    // Set SELECT column to be very narrow
    pimpl_->page_->table()->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    pimpl_->page_->table()->setColumnWidth(0, 60);


    pimpl_->bulk_widget_ = new QWidget;
    auto* bulk_row = new QHBoxLayout(pimpl_->bulk_widget_);
    bulk_row->setContentsMargins(0, 0, 0, 0);
    bulk_row->setSpacing(10);
    
    auto create_bulk_btn = [this](const QString& text, const QString& iconType, const QString& color, auto validate_func, auto func, const QString& confirm_msg = "") {
        auto* btn = new HoverIconButton(text, iconType, color);
        btn->setObjectName("BulkActionBtn_" + iconType);
        btn->setIconSize(QSize(14, 14));
        btn->setFixedSize(100, 30);
        btn->setCursor(Qt::PointingHandCursor);
        
        connect(btn, &QPushButton::clicked, this, [this, validate_func, func, confirm_msg]() {
            auto selected = pimpl_->get_selected_containers();
            if (selected.isEmpty()) return;
            
            QString error_msg = validate_func(selected);
            if (!error_msg.isEmpty()) {
                CustomAlert alert(CustomAlert::Warning, "Action Not Allowed", error_msg, this);
                alert.exec();
                return;
            }

            if (!confirm_msg.isEmpty()) {
                CustomAlert alert(CustomAlert::Question, "Confirm", confirm_msg, this);
                if (alert.exec() != QDialog::Accepted) {
                    return;
                }
            }
            
            QStringList ids;
            for (const auto& s : selected) ids << s.first;
            
            pimpl_->overlay_->show();
            pimpl_->overlay_->raise();
            QTimer::singleShot(50, this, [this, ids, func]() {
                func(ids);
                QTimer::singleShot(2500, this, [this]() {
                    refresh();
                    pimpl_->overlay_->hide();
                });
            });
        });
        return btn;
    };
    
    auto val_start = [](const QList<QPair<QString, QString>>& sel) -> QString {
        for (const auto& s : sel) {
            if (s.second == "running" || s.second == "paused") return "Cannot start container " + s.first + " because it is already running or paused.";
        }
        return "";
    };
    auto val_stop = [](const QList<QPair<QString, QString>>& sel) -> QString {
        for (const auto& s : sel) {
            if (s.second != "running" && s.second != "paused") return "Cannot stop container " + s.first + " because it is already stopped.";
        }
        return "";
    };
    auto val_unpause = [](const QList<QPair<QString, QString>>& sel) -> QString {
        for (const auto& s : sel) {
            if (s.second != "paused") return "Cannot unpause container " + s.first + " because it is not paused.";
        }
        return "";
    };
    auto val_pause = [](const QList<QPair<QString, QString>>& sel) -> QString {
        for (const auto& s : sel) {
            if (s.second != "running") return "Cannot pause container " + s.first + " because it is not running.";
        }
        return "";
    };
    auto val_delete = [](const QList<QPair<QString, QString>>& sel) -> QString {
        return ""; 
    };

    bulk_row->addWidget(create_bulk_btn("Start", "play", "#2ea043", val_start, [](const QStringList& ids){ Backend::get_instance().start_container(ids); }));
    bulk_row->addWidget(create_bulk_btn("Stop", "stop", "#f85149", val_stop, [](const QStringList& ids){ Backend::get_instance().stop_container(ids); }));
    bulk_row->addWidget(create_bulk_btn("Unpause", "play", "#2ea043", val_unpause, [](const QStringList& ids){ Backend::get_instance().unpause_container(ids); }));
    bulk_row->addWidget(create_bulk_btn("Pause", "pause", "#f97316", val_pause, [](const QStringList& ids){ Backend::get_instance().pause_container(ids); }));
    bulk_row->addWidget(create_bulk_btn("Delete", "delete", "#f85149", val_delete, [](const QStringList& ids){ Backend::get_instance().delete_container(ids); }, "Are you sure you want to delete the selected containers?"));
    
    pimpl_->bulk_widget_->hide(); // Hidden by default
    pimpl_->page_->header_layout()->addStretch();
    pimpl_->page_->header_layout()->addWidget(pimpl_->bulk_widget_);
    
    auto* prune_btn = new HoverIconButton("Prune", "prune", "#f85149");
    prune_btn->setObjectName("BulkActionBtn_prune");
    prune_btn->setIconSize(QSize(14, 14));
    prune_btn->setFixedSize(100, 30);
    prune_btn->setCursor(Qt::PointingHandCursor);
    prune_btn->setToolTip("Delete all exited/stopped containers");
    connect(prune_btn, &QPushButton::clicked, this, [this]() {
        CustomAlert alert(CustomAlert::Question, "Confirm Prune", "Are you sure you want to prune all stopped/exited containers?", this);
        if (alert.exec() != QDialog::Accepted) {
            return;
        }
        pimpl_->overlay_->show();
        pimpl_->overlay_->raise();
        QTimer::singleShot(50, this, [this]() {
            Backend::get_instance().prune_containers();
            QTimer::singleShot(2500, this, [this]() {
                refresh();
                pimpl_->overlay_->hide();
            });
        });
    });
    pimpl_->page_->header_layout()->addWidget(prune_btn);

    connect(pimpl_->page_, &TablePage::add_clicked, this, [this]() {
        open_create_dialog();
    });

    // Create the overlay for loading
    pimpl_->overlay_ = new QWidget(this);
    pimpl_->overlay_->setFixedSize(160, 40);
    pimpl_->overlay_->setStyleSheet("background-color: #27272A; border: 1px solid #3F3F46; border-radius: 20px;");
    
    auto* overlay_layout = new QHBoxLayout(pimpl_->overlay_);
    overlay_layout->setContentsMargins(20, 0, 20, 0);
    overlay_layout->setSpacing(10);
    
    auto* spinner_lbl = new QLabel;
    spinner_lbl->setStyleSheet("color: #F97316; font-size: 18px; font-weight: bold; background: transparent; border: none;");
    
    pimpl_->loading_text_ = new QLabel("Processing...");
    pimpl_->loading_text_->setStyleSheet("color: #FAFAFA; font-size: 13px; font-weight: bold; background: transparent; border: none;");
    
    overlay_layout->addWidget(spinner_lbl);
    overlay_layout->addWidget(pimpl_->loading_text_);
    
    auto* timer = new QTimer(pimpl_->overlay_);
    connect(timer, &QTimer::timeout, pimpl_->overlay_, [spinner_lbl]() {
        static int frame = 0;
        const QString frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        spinner_lbl->setText(frames[frame]);
        frame = (frame + 1) % 10;
    });
    timer->start(80);
    
    pimpl_->overlay_->hide();

    root->addWidget(pimpl_->page_);
    
    QTimer::singleShot(100, this, &ContainersPage::refresh);
}

auto ContainersPage::open_create_dialog() -> void {
    CreateDialog d(this);
    if (d.exec() == QDialog::Accepted) {
        QString name { d.get_container_name() };
        QString img  { d.get_container_image() };
        if (name.trimmed().isEmpty()) name = "new-container";
        if (img.trimmed().isEmpty())  img  = "ubuntu:latest";
        QString id { QString::number(QRandomGenerator::global()->generate(), 16).right(6) };
        
        Container c;
        c.id = id;
        c.name = name;
        c.image = img;
        c.status = "running";
        c.filesystem = d.get_filesystem();
        c.devices = d.get_devices();
        c.volumes = d.get_volumes();
        c.ports = d.get_ports();
        c.prevent_interaction = d.get_prevent_interaction();
        c.options = d.get_options();
        c.command = d.get_command();
        c.cpu_quota = d.get_cpu_quota();
        c.cpu_weight = d.get_cpu_weight();
        c.memory_max = d.get_memory_max();
        c.memory_swap = d.get_memory_swap();
        c.pids_limit = d.get_pids_limit();
        c.cpuset_cpus = d.get_cpuset_cpus();
        c.cpuset_mems = d.get_cpuset_mems();
        c.io_weight = d.get_io_weight();
        c.io_max = d.get_io_max();
        
        Backend::get_instance().add_container(c);
        refresh();
    }
}

ContainersPage::~ContainersPage() = default;

void ContainersPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (pimpl_->overlay_) {
        pimpl_->overlay_->move((width() - pimpl_->overlay_->width()) / 2, height() - pimpl_->overlay_->height() - 40);
    }
}

auto ContainersPage::refresh() -> void {
    int v_scroll = pimpl_->page_->table()->verticalScrollBar()->value();
    QSet<QString> checked_ids;
    for (int r = 0; r < pimpl_->page_->table()->rowCount(); ++r) {
        auto* cell = pimpl_->page_->table()->cellWidget(r, 0);
        if (cell) {
            auto* cb = cell->findChild<QPushButton*>("SelectCheckbox");
            if (cb && cb->isChecked()) {
                auto* item = pimpl_->page_->table()->item(r, 1);
                if (item) checked_ids.insert(item->text());
            }
        }
    }

    pimpl_->page_->table()->setRowCount(0);
    
    auto containers = Backend::get_instance().get_containers();
    QString error_msg = Backend::get_instance().get_last_error();
    if (!error_msg.isEmpty()) {
        static bool showing_error = false;
        if (!showing_error) {
            showing_error = true;
            CustomAlert alert(CustomAlert::Warning, "Backend Error", error_msg, this);
            alert.exec();
            showing_error = false;
        }
    }
    
    int total = containers.size();
    int running = 0;
    int stopped = 0;
    int paused = 0;
    
    for (const auto& c : containers) {
        if (c.status == "running") running++;
        else if (c.status == "paused") paused++;
        else stopped++;
        
        int row = pimpl_->page_->table()->rowCount();
        pimpl_->page_->table()->insertRow(row);
        
        auto* cb_cell = new QWidget;
        auto* cb_layout = new QHBoxLayout(cb_cell);
        cb_layout->setContentsMargins(0, 0, 0, 0);
        cb_layout->setAlignment(Qt::AlignCenter);
        
        auto* cb = new QPushButton;
        cb->setObjectName("SelectCheckbox");
        cb->setCheckable(true);
        cb->setFixedSize(16, 16);
        cb->setCursor(Qt::PointingHandCursor);
        cb->setStyleSheet(
            "QPushButton { border: 1px solid #71717a; border-radius: 4px; background: transparent; color: transparent; font-weight: bold; font-size: 11px; padding-bottom: 2px; } "
            "QPushButton:checked { background: #f97316; border: 1px solid #f97316; color: black; }"
        );
        if (checked_ids.contains(c.id)) {
            cb->setChecked(true);
            cb->setText("✓");
        }
        connect(cb, &QPushButton::toggled, cb, [cb](bool checked) {
            cb->setText(checked ? "✓" : "");
        });
        connect(cb, &QPushButton::toggled, this, [this]() {
            pimpl_->update_bulk_actions_visibility();
        });
        
        cb_layout->addWidget(cb);
        pimpl_->page_->table()->setCellWidget(row, 0, cb_cell);
        
        auto* id_item = make_item(c.id, true);
        id_item->setData(Qt::UserRole, c.status);
        pimpl_->page_->table()->setItem(row, 1, id_item);
        pimpl_->page_->table()->setItem(row, 2, make_item(c.name));
        pimpl_->page_->table()->setItem(row, 3, make_item(c.image));
        pimpl_->page_->table()->setCellWidget(row, 4, make_status_badge(c.status));
        
        auto* cell = new QWidget;
        auto* h = new QHBoxLayout(cell);
        h->setContentsMargins(5, 0, 5, 0);
        h->setSpacing(5);
        h->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        
        auto create_btn = [this, c](const QString& iconType, const QString& tooltip, const QString& color, auto func, const QString& confirm_msg = "") {
            auto* btn = new HoverIconButton("", iconType, color);
            btn->setObjectName("RowActionBtn_" + iconType);
            btn->setIconSize(QSize(14, 14));
            btn->setToolTip(tooltip);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedSize(30, 28);
            connect(btn, &QPushButton::clicked, this, [this, c, func, confirm_msg]() {
                if (!confirm_msg.isEmpty()) {
                    CustomAlert alert(CustomAlert::Question, "Confirm", confirm_msg, this);
                    if (alert.exec() != QDialog::Accepted) {
                        return;
                    }
                }
                pimpl_->overlay_->show();
                pimpl_->overlay_->raise(); // Ensure overlay is on top
                QTimer::singleShot(50, this, [this, c, func]() {
                    func(QStringList() << c.id);
                    // refresh after command finishes (allow 2.5s for async execution)
                    QTimer::singleShot(2500, this, [this]() {
                        refresh();
                        pimpl_->overlay_->hide();
                    });
                });
            });
            return btn;
        };

        if (c.status == "running") {
            h->addWidget(create_btn("pause", "Pause", "#f97316", [](const QStringList& ids){ Backend::get_instance().pause_container(ids); }));
            h->addWidget(create_btn("stop", "Stop", "#f85149", [](const QStringList& ids){ Backend::get_instance().stop_container(ids); }));
        } else if (c.status == "paused") {
            h->addWidget(create_btn("play", "Unpause", "#2ea043", [](const QStringList& ids){ Backend::get_instance().unpause_container(ids); }));
            h->addWidget(create_btn("stop", "Stop", "#f85149", [](const QStringList& ids){ Backend::get_instance().stop_container(ids); }));
        } else {
            h->addWidget(create_btn("play", "Start", "#2ea043", [](const QStringList& ids){ Backend::get_instance().start_container(ids); }));
        }

        auto* copy_btn = new HoverIconButton("", "copy", "#a1a1aa");
        copy_btn->setObjectName("RowActionBtn_copy");
        copy_btn->setIconSize(QSize(14, 14));
        copy_btn->setToolTip("Copy full ID");
        copy_btn->setCursor(Qt::PointingHandCursor);
        copy_btn->setFixedSize(30, 28);
        connect(copy_btn, &QPushButton::clicked, this, [c]() {
            QApplication::clipboard()->setText(c.id);
        });
        h->addWidget(copy_btn);
        
        h->addWidget(create_btn("delete", "Delete", "#f85149", [](const QStringList& ids){ Backend::get_instance().delete_container(ids); }, "Are you sure you want to delete this container?"));

        auto* update_btn = new HoverIconButton("", "refresh", "#58a6ff");
        update_btn->setObjectName("RowActionBtn_update");
        update_btn->setIconSize(QSize(14, 14));
        update_btn->setToolTip("Update Container");
        update_btn->setCursor(Qt::PointingHandCursor);
        update_btn->setFixedSize(30, 28);
        connect(update_btn, &QPushButton::clicked, this, [c, this]() {
            UpdateDialog d(c, this);
            if (d.exec() == QDialog::Accepted) {
                Quiver::Container updated = c;
                updated.cpu_quota = d.get_cpu_quota();
                updated.cpu_weight = d.get_cpu_weight();
                updated.memory_max = d.get_memory_max();
                updated.memory_swap = d.get_memory_swap();
                updated.pids_limit = d.get_pids_limit();
                updated.cpuset_cpus = d.get_cpuset_cpus();
                updated.cpuset_mems = d.get_cpuset_mems();
                updated.io_weight = d.get_io_weight();
                updated.io_max = d.get_io_max();
                Backend::get_instance().update_container(updated);
            }
        });
        h->addWidget(update_btn);

        auto* attach_btn = new HoverIconButton("", "attach", "#58a6ff");
        attach_btn->setObjectName("RowActionBtn_attach");
        attach_btn->setIconSize(QSize(14, 14));
        attach_btn->setToolTip("Attach to Terminal");
        attach_btn->setCursor(Qt::PointingHandCursor);
        attach_btn->setFixedSize(30, 28);
        connect(attach_btn, &QPushButton::clicked, this, [c]() {
            QString binPath = Backend::get_instance().get_cli_path();
            QString workDir = QFileInfo(binPath).absolutePath();
            QString cmd = QString("alacritty -e sh -c 'cd %1 && ./quiver attach %2; echo \"\n[Process Exited]\"; read -p \"Press Enter to close...\"'")
                            .arg(workDir, c.id);
            QProcess::startDetached("sh", QStringList() << "-c" << cmd);
        });
        h->addWidget(attach_btn);

        auto* info_btn = new HoverIconButton("", "info", "#a1a1aa");
        info_btn->setObjectName("RowActionBtn_info");
        info_btn->setIconSize(QSize(14, 14));
        info_btn->setToolTip("View Information");
        info_btn->setCursor(Qt::PointingHandCursor);
        info_btn->setFixedSize(30, 28);
        connect(info_btn, &QPushButton::clicked, this, [this, c]() {
            emit container_info_requested(c.id);
        });
        h->addWidget(info_btn);

        pimpl_->page_->table()->setCellWidget(row, 5, cell);
    }
    
    pimpl_->stat_total_->set_value(QString::number(total));
    pimpl_->stat_running_->set_value(QString::number(running));
    pimpl_->stat_stopped_->set_value(QString::number(stopped));
    pimpl_->stat_paused_->set_value(QString::number(paused));
    
    pimpl_->page_->table()->verticalScrollBar()->setValue(v_scroll);
    pimpl_->update_bulk_actions_visibility();
}

struct ImagesPage::Impl { TablePage* page_ {}; };

ImagesPage::ImagesPage(QWidget* parent)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{
    auto* root { new QVBoxLayout(this) };
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    make_stat_row(
        "TOTAL IMAGES", "6",
        "AVAILABLE",    "4", "#4ade80",
        "UNUSED",       "2", "#fb7185",
        root);

    pimpl_->page_ = new TablePage(
        "Images",
        { "REPOSITORY", "TAG",  "SIZE", "CREATED" },
        this);

    auto* build_btn = new QPushButton("Build Image");
    build_btn->setStyleSheet(
        "QPushButton { background: #f97316; color: #ffffff; border: none; border-radius: 6px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background: #ea580c; }"
    );
    build_btn->setFixedSize(120, 36);
    build_btn->setCursor(Qt::PointingHandCursor);
    
    connect(build_btn, &QPushButton::clicked, this, [this]() {
        BuildImageDialog dialog(this);
        dialog.exec();
    });

    pimpl_->page_->header_layout()->addStretch();
    pimpl_->page_->header_layout()->addWidget(build_btn);

    auto add = [&](const QStringList& d) {
        pimpl_->page_->add_row(d, "Delete", "TableDangerBtn");
    };
    add({ "nginx",       "alpine",     "23.5 MB", "2 days ago"  });
    add({ "redis",       "6.2",      "113 MB",  "1 week ago"  });
    add({ "postgres",   "14",       "376 MB",  "3 days ago"  });
    add({ "ubuntu",    "22.04",      "77.8 MB", "5 days ago"  });
    add({ "node",        "18-alpine",  "168 MB",  "1 day ago"   });
    add({ "python",    "3.11-slim",  "125 MB",  "4 days ago"  });

    connect(pimpl_->page_, &TablePage::add_clicked, this, [this]() {
        QDialog d(this);
        d.setObjectName("CreateDialog");
        d.setFixedSize(400, 180);
        d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        auto* l { new QVBoxLayout(&d) };
        l->setContentsMargins(28, 28, 28, 28); l->setSpacing(14);
        auto* title { new QLabel("Pull Image") }; title->setObjectName("PageTitle"); l->addWidget(title);
        auto* r { new QHBoxLayout };
        auto* name_in { new QLineEdit }; name_in->setPlaceholderText("e.g. nginx"); name_in->setFixedHeight(34);
        auto* tag_in  { new QLineEdit }; tag_in->setPlaceholderText("latest"); tag_in->setFixedWidth(90); tag_in->setFixedHeight(34);
        r->addWidget(name_in); r->addWidget(new QLabel(":")); r->addWidget(tag_in);
        l->addLayout(r); l->addStretch();
        auto* btns { new QHBoxLayout };
        auto* cancel { new QPushButton("Cancel") }; 
        cancel->setObjectName("SecondaryBtn"); 
        cancel->setCursor(Qt::PointingHandCursor);
        cancel->setFixedSize(85, 34);
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        auto* pull { new QPushButton("Pull") };
         pull->setObjectName("PrimaryButton"); 
        pull->setCursor(Qt::PointingHandCursor);
        pull->setFixedSize(85, 34);
        connect(pull, &QPushButton::clicked, &d, &QDialog::accept);
        btns->addStretch(); btns->addWidget(cancel); btns->addWidget(pull);
        l->addLayout(btns);
        if (d.exec() == QDialog::Accepted) {
            QString name { name_in->text().trimmed() };
            if (name.isEmpty()) return;
            QString tag { tag_in->text().trimmed() };
        }
    });

    root->addWidget(pimpl_->page_, 1);
}
ImagesPage::~ImagesPage() = default;

struct VolumesPage::Impl { TablePage* page_ {}; };

VolumesPage::VolumesPage(QWidget* parent)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{
    auto* root { new QVBoxLayout(this) };
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    make_stat_row(
        "TOTAL VOLUMES", "4",
        "MOUNTED",       "3", "#4ade80",
        "UNMOUNTED",     "1", "#fb7185",
        root);

    pimpl_->page_ = new TablePage(
        "Volumes",
        { "NAME", "DRIVER", "MOUNT POINT", "SIZE", "CREATED" },
        this);

    auto add = [&](const QStringList& d) {
        pimpl_->page_->add_row(d);
    };
    add({ "postgres_data",   "local", "/var/lib/docker/volumes/postgres_data", "2.3 GB",  "3 days ago" });
    add({ "redis_cache",    "local", "/var/lib/docker/volumes/redis_cache",   "128 MB",  "1 week ago" });
    add({ "nginx_conf",     "local", "/var/lib/docker/volumes/nginx_conf",    "4.2 MB",  "5 days ago" });
    add({ "app_uploads",    "nfs",   "/mnt/nfs/uploads",                      "14.7 GB", "2 days ago" });

    connect(pimpl_->page_, &TablePage::add_clicked, this, [this]() {
        QDialog d(this);
        d.setObjectName("CreateDialog");
        d.setFixedSize(400, 200);
        d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        auto* l { new QVBoxLayout(&d) };
        l->setContentsMargins(28, 28, 28, 28); l->setSpacing(14);
        auto* title { new QLabel("Create Volume") }; title->setObjectName("PageTitle"); l->addWidget(title);
        auto* name_in { new QLineEdit }; name_in->setPlaceholderText("volume-name"); name_in->setFixedHeight(34); l->addWidget(name_in);
        auto* driver { new QComboBox }; driver->addItems({"local", "nfs", "tmpfs"}); driver->setFixedHeight(34); l->addWidget(driver);
        l->addStretch();
        auto* btns { new QHBoxLayout };
        auto* cancel { new QPushButton("Cancel") }; 
        cancel->setObjectName("SecondaryBtn"); cancel->setCursor(Qt::PointingHandCursor);
        cancel->setFixedSize(85, 34);
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        auto* create { new QPushButton("Create") }; create->setObjectName("PrimaryButton"); create->setCursor(Qt::PointingHandCursor);
        create->setFixedSize(85, 34);
        connect(create, &QPushButton::clicked, &d, &QDialog::accept);
        btns->addStretch(); btns->addWidget(cancel); btns->addWidget(create);
        l->addLayout(btns);
        if (d.exec() == QDialog::Accepted) {
            QString name { name_in->text().trimmed() };
            if (name.isEmpty()) return;
            pimpl_->page_->add_row(
                { name, "mounted", driver->currentText(),
                 "/var/lib/docker/volumes/" + name, "0 B", "just now" },
                "Unmount", "TableDangerBtn");
        }
    });

    root->addWidget(pimpl_->page_, 1);
}
VolumesPage::~VolumesPage() = default;


struct PortsPage::Impl { 
    TablePage* page_ {}; 
    Quiver::StatCard* stat_total_ {};
    Quiver::StatCard* stat_open_ {};
    Quiver::StatCard* stat_closed_ {};
};

PortsPage::PortsPage(QWidget* parent)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{
    auto* root { new QVBoxLayout(this) };
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* stats_row { new QHBoxLayout };
    stats_row->setSpacing(20);
    stats_row->setContentsMargins(0, 0, 0, 24);
    pimpl_->stat_total_ = new Quiver::StatCard("TOTAL PORTS", "0", "#F97316");
    pimpl_->stat_open_ = new Quiver::StatCard("OPEN", "0", "#4ade80");
    pimpl_->stat_closed_ = new Quiver::StatCard("CLOSED", "0", "#fb7185");
    stats_row->addWidget(pimpl_->stat_total_);
    stats_row->addWidget(pimpl_->stat_open_);
    stats_row->addWidget(pimpl_->stat_closed_);
    root->addLayout(stats_row);

    pimpl_->page_ = new TablePage(
        "Ports",
        { "CONTAINER ID", "TCP PORTS", "UDP PORTS" },
        this, false);
    
    // Set a reasonable fixed width for Container ID
    // pimpl_->page_->table()->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    pimpl_->page_->table()->setColumnWidth(0, 250);


    connect(pimpl_->page_, &TablePage::add_clicked, this, [this]() {
        QDialog d(this);
        d.setObjectName("CreateDialog");
        d.setFixedSize(420, 230);
        d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        auto* l { new QVBoxLayout(&d) };
        l->setContentsMargins(28, 28, 28, 28); l->setSpacing(14);
        auto* title { new QLabel("Add Port Mapping") }; title->setObjectName("PageTitle"); l->addWidget(title);
        auto* r { new QHBoxLayout };
        auto* host_in { new QLineEdit }; host_in->setPlaceholderText("Host port"); host_in->setFixedHeight(34);
        auto* cont_in { new QLineEdit }; cont_in->setPlaceholderText("Container port"); cont_in->setFixedHeight(34);
        r->addWidget(host_in); r->addWidget(new QLabel("→")); r->addWidget(cont_in);
        l->addLayout(r);
        auto* proto { new QComboBox }; proto->addItems({"TCP", "UDP"}); proto->setFixedHeight(34); l->addWidget(proto);
        l->addStretch();
        auto* btns { new QHBoxLayout };
        auto* cancel { new QPushButton("Cancel") }; cancel->setObjectName("SecondaryBtn"); cancel->setCursor(Qt::PointingHandCursor);
        cancel->setFixedSize(85, 34);
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        auto* add_btn { new QPushButton("Add") }; add_btn->setObjectName("PrimaryButton"); add_btn->setCursor(Qt::PointingHandCursor);
        add_btn->setFixedSize(85, 34);
        connect(add_btn, &QPushButton::clicked, &d, &QDialog::accept);
        btns->addStretch(); btns->addWidget(cancel); btns->addWidget(add_btn);
        l->addLayout(btns);
        if (d.exec() == QDialog::Accepted) {
            QString host { host_in->text().trimmed() };
            QString cont { cont_in->text().trimmed() };
            if (host.isEmpty() || cont.isEmpty()) return;
            pimpl_->page_->add_row(
                { "—", "open", host, cont, proto->currentText(), "0.0.0.0" },
                "Delete", "TableDangerBtn");
        }
    });

    root->addWidget(pimpl_->page_, 1);
    
    QTimer::singleShot(100, this, &PortsPage::refresh);
}
PortsPage::~PortsPage() = default;

auto PortsPage::refresh() -> void {
    while (pimpl_->page_->table()->rowCount() > 0) {
        pimpl_->page_->table()->removeRow(0);
    }
    
    auto ports = Backend::get_instance().get_ports();
    
    int total = ports.size();
    int open = 0;
    int closed = 0;
    
    for (const auto& p : ports) {
        if (!p.tcp.isEmpty() || !p.udp.isEmpty()) open++;
        else closed++;
        
        int row = pimpl_->page_->table()->rowCount();
        pimpl_->page_->table()->insertRow(row);
        
        pimpl_->page_->table()->setItem(row, 0, make_item(p.id, true));
        if (p.tcp.isEmpty() && p.udp.isEmpty()) {
            pimpl_->page_->table()->setItem(row, 1, make_item("no ports forwarded"));
            pimpl_->page_->table()->setItem(row, 2, make_item("no ports forwarded"));
        } else {
            pimpl_->page_->table()->setItem(row, 1, make_item(p.tcp));
            pimpl_->page_->table()->setItem(row, 2, make_item(p.udp));
        }
    }
    
    pimpl_->stat_total_->set_value(QString::number(total));
    pimpl_->stat_open_->set_value(QString::number(open));
    pimpl_->stat_closed_->set_value(QString::number(closed));
}

struct DevicesPage::Impl { TablePage* page_ {}; };

DevicesPage::DevicesPage(QWidget* parent)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{
    auto* root { new QVBoxLayout(this) };
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    make_stat_row(
        "TOTAL DEVICES", "5",
        "ACTIVE",        "4", "#4ade80",
        "INACTIVE",      "1", "#fb7185",
        root);

    pimpl_->page_ = new TablePage(
        "Devices",
        { "DEVICE PATH", "STATUS", "TYPE", "CONTAINER", "PERMISSIONS", "DRIVER" },
        this);

    auto add = [&](const QStringList& d) {
        pimpl_->page_->add_row(d, "Remove", "TableDangerBtn");
    };
    add({ "/dev/ttyUSB0",   "active",   "Serial",  "nginx-proxy", "rw",  "usbserial" });
    add({ "/dev/video0",    "active",   "Camera",  "app-server",  "rw",  "v4l2"      });
    add({ "/dev/dri/card0", "active",   "GPU",     "ml-worker",   "rwm", "drm"       });
    add({ "/dev/snd",       "inactive", "Audio",   "—",           "rw",  "alsa"      });
    add({ "/dev/sda1",      "active",   "Storage", "postgres-db", "ro",  "block"     });

    connect(pimpl_->page_, &TablePage::add_clicked, this, [this]() {
        QDialog d(this);
        d.setObjectName("CreateDialog");
        d.setFixedSize(420, 230);
        d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        auto* l { new QVBoxLayout(&d) };
        l->setContentsMargins(28, 28, 28, 28); l->setSpacing(14);
        auto* title { new QLabel("Add Device") }; title->setObjectName("PageTitle"); l->addWidget(title);
        auto* path_in { new QLineEdit }; path_in->setPlaceholderText("/dev/ttyUSB0"); path_in->setFixedHeight(34); l->addWidget(path_in);
        auto* r { new QHBoxLayout };
        auto* type_cb { new QComboBox }; type_cb->addItems({"Serial","Camera","GPU","Audio","Storage","Other"}); type_cb->setFixedHeight(34);
        auto* perm_cb { new QComboBox }; perm_cb->addItems({"rw","ro","rwm"}); perm_cb->setFixedHeight(34);
        r->addWidget(type_cb); r->addWidget(perm_cb);
        l->addLayout(r); l->addStretch();
        auto* btns { new QHBoxLayout };
        auto* cancel { new QPushButton("Cancel") }; cancel->setObjectName("SecondaryBtn"); cancel->setCursor(Qt::PointingHandCursor); cancel->setFixedSize(85, 34);
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        auto* add_btn { new QPushButton("Add") }; add_btn->setObjectName("PrimaryButton"); add_btn->setCursor(Qt::PointingHandCursor); add_btn->setFixedSize(85, 34);
        connect(add_btn, &QPushButton::clicked, &d, &QDialog::accept);
        btns->addStretch(); btns->addWidget(cancel); btns->addWidget(add_btn);
        l->addLayout(btns);
        if (d.exec() == QDialog::Accepted) {
            QString path { path_in->text().trimmed() };
            if (path.isEmpty()) return;
            pimpl_->page_->add_row(
                { path, "active", type_cb->currentText(), "—", perm_cb->currentText(), "—" },
                "Remove", "TableDangerBtn");
        }
    });

    root->addWidget(pimpl_->page_, 1);
}
DevicesPage::~DevicesPage() = default;


struct SettingsPage::Impl {};

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{

    
  
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);


    auto* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
 
    auto* scroll_content = new QWidget;
    scroll_content->setStyleSheet("QWidget { background: transparent; }"); 
 
    auto* layout = new QVBoxLayout(scroll_content);
    layout->setContentsMargins(0, 0, 20, 0); 
    layout->setSpacing(24);

    auto* title { new QLabel("Settings") };
    title->setObjectName("PageTitle");
    layout->addWidget(title);

   
    auto* acc_frame { new QFrame };
    acc_frame->setObjectName("SettingsGroup");
    auto* acc_fl { new QVBoxLayout(acc_frame) };
    acc_fl->setContentsMargins(24, 20, 24, 20);
    acc_fl->setSpacing(16);
    
    auto* acc_gt { new QLabel("ACCOUNT PROFILE") }; 
    acc_gt->setObjectName("SettingsGroupTitle"); 
    acc_fl->addWidget(acc_gt);
    
    auto* acc_div { new QFrame }; 
    acc_div->setObjectName("Divider"); 
    acc_div->setFixedHeight(1); 
    acc_fl->addWidget(acc_div);

    auto* profile_row { new QHBoxLayout };
    profile_row->setSpacing(30);

 
    auto* avatar_box { new QVBoxLayout };
    auto* avatar_display { new QLabel };
    avatar_display->setFixedSize(90, 90);
    avatar_display->setObjectName("AvatarDisplay");
    avatar_display->setAlignment(Qt::AlignCenter);
    
    auto get_circular_pixmap = [](const QString& path, int size) -> QPixmap {
        QPixmap src(path);
        if (src.isNull()) src.load(":/assets/icons/profile.svg"); 
        
       
        QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        
     
        QPixmap cropped = scaled.copy((scaled.width() - size) / 2, (scaled.height() - size) / 2, size, size);

        QPixmap out(size, size);
        out.fill(Qt::transparent);
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath path_clip;
        path_clip.addEllipse(0, 0, size, size);
        p.setClipPath(path_clip);
        p.drawPixmap(0, 0, cropped);
        return out;
    };

    avatar_display->setPixmap(get_circular_pixmap(AuthManager::get_instance().get_cached_avatar_path(), 86)); 
    
    auto* upload_btn { new QPushButton("Upload Image") };
    upload_btn->setObjectName("SecondaryBtn");
    upload_btn->setCursor(Qt::PointingHandCursor);
    upload_btn->setFixedWidth(100);
    
    avatar_box->addWidget(avatar_display, 0, Qt::AlignHCenter);
    avatar_box->addWidget(upload_btn, 0, Qt::AlignHCenter);

    profile_row->addLayout(avatar_box);

  
    auto* form_box { new QVBoxLayout };
    form_box->setSpacing(10);
    
    auto* name_lbl { new QLabel("Full Name") }; name_lbl->setObjectName("SettingsLabel");
    auto* name_input { new QLineEdit };
    name_input->setText(AuthManager::get_instance().get_full_name());
    name_input->setFixedHeight(36);
    
    auto* user_lbl { new QLabel("Username") }; user_lbl->setObjectName("SettingsLabel");
    auto* user_input { new QLineEdit };
    user_input->setText(AuthManager::get_instance().get_username());
    user_input->setFixedHeight(36);
    
    connect(&AuthManager::get_instance(), &AuthManager::profile_updated, this, [name_input, user_input]() {
        name_input->setText(AuthManager::get_instance().get_full_name());
        user_input->setText(AuthManager::get_instance().get_username());
    });
    
    auto* save_btn { new QPushButton("Save Changes") };
    save_btn->setObjectName("PrimaryButton");
    save_btn->setCursor(Qt::PointingHandCursor);
    save_btn->setFixedWidth(140);
    
    form_box->addWidget(name_lbl);
    form_box->addWidget(name_input);
    form_box->addWidget(user_lbl);
    form_box->addWidget(user_input);
    form_box->addWidget(save_btn, 0, Qt::AlignRight);
    
    profile_row->addLayout(form_box);
    acc_fl->addLayout(profile_row);
    layout->addWidget(acc_frame);



    layout->addStretch(); 
    scroll_area->setWidget(scroll_content);
    main_layout->addWidget(scroll_area);


connect(upload_btn, &QPushButton::clicked, this, [avatar_display, get_circular_pixmap, this]() {
        QString file_name = QFileDialog::getOpenFileName(
            this, "Select Profile Picture", "", "Images (*.png *.jpg *.jpeg *.webp)");
        if (file_name.isEmpty()) return;

       
        avatar_display->setPixmap(get_circular_pixmap(file_name, 86));

      
        AuthManager::get_instance().upload_avatar(file_name);
    });

    
    connect(&AuthManager::get_instance(), &AuthManager::profile_updated, this,
        [avatar_display, get_circular_pixmap]() {
            
            QString latest_avatar = AuthManager::get_instance().get_cached_avatar_path();
            
            
            if (!latest_avatar.isEmpty()) {
                avatar_display->setPixmap(get_circular_pixmap(latest_avatar, 86));
            } else {
                avatar_display->setPixmap(get_circular_pixmap(":/assets/icons/profile.svg", 86));
            }
        });

connect(save_btn, &QPushButton::clicked, this, [name_input, user_input, save_btn]() {
    save_btn->setText("Saving...");
    save_btn->setEnabled(false);
    AuthManager::get_instance().update_profile(name_input->text(), user_input->text());
});
connect(&AuthManager::get_instance(), &AuthManager::profile_updated, this, [save_btn]() {
    save_btn->setText("Saved!");
    save_btn->setEnabled(true);
    QTimer::singleShot(2000, save_btn, [save_btn]() { save_btn->setText("Save Changes"); });
});
    
      auto* app_frame { new QFrame };
    app_frame->setObjectName("SettingsGroup");
    auto* app_fl { new QVBoxLayout(app_frame) };
    app_fl->setContentsMargins(24, 20, 24, 20);
    app_fl->setSpacing(16);

    auto* app_gt { new QLabel("APPEARANCE") }; 
    app_gt->setObjectName("SettingsGroupTitle"); 
    app_fl->addWidget(app_gt);
    
    auto* app_div { new QFrame }; 
    app_div->setObjectName("Divider"); 
    app_div->setFixedHeight(1); 
    app_fl->addWidget(app_div);

    
    auto* theme_row { new QHBoxLayout };
    theme_row->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); 

    auto* theme_lbl { new QWidget }; 
    
    
    theme_lbl->setFixedWidth(120); 

    
    auto* btn_light { new QToolButton };
    btn_light->setObjectName("ThemeLightCard");
    btn_light->setText("Light Mode");
    btn_light->setIcon(QIcon(":/assets/icons/light_mode.png")); 
    btn_light->setIconSize(QSize(440, 240)); 
    btn_light->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn_light->setCheckable(true);
    btn_light->setAutoExclusive(true);
    btn_light->setCursor(Qt::PointingHandCursor);
    btn_light->setFixedSize(450, 300); 

    
    auto* btn_dark { new QToolButton };
    btn_dark->setObjectName("ThemeDarkCard");
    btn_dark->setText("Dark Mode");
    btn_dark->setIcon(QIcon(":/assets/icons/dark_mode.png")); 
    btn_dark->setIconSize(QSize(440, 240)); 
    btn_dark->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn_dark->setCheckable(true);
    btn_light->setAutoExclusive(true);
    btn_dark->setChecked(true); 
    btn_dark->setCursor(Qt::PointingHandCursor);
    btn_dark->setFixedSize(450, 300); 


   auto* right_spacer { new QWidget };
    right_spacer->setFixedWidth(120); 
    theme_row->addWidget(theme_lbl);     
    theme_row->addStretch(1);            
    theme_row->addWidget(btn_light);     
    theme_row->addSpacing(60);           
    theme_row->addWidget(btn_dark);      
    theme_row->addStretch(1);            
    theme_row->addWidget(right_spacer);  

    app_fl->addLayout(theme_row);
    layout->addWidget(app_frame);

    auto make_group = [&](const QString& group_title,
                          std::initializer_list<QPair<QString,QString>> items) {
        auto* frame { new QFrame };
        frame->setObjectName("SettingsGroup");
        auto* fl { new QVBoxLayout(frame) };
        fl->setContentsMargins(24, 20, 24, 20);
        fl->setSpacing(16);
        auto* gt { new QLabel(group_title) }; gt->setObjectName("SettingsGroupTitle"); fl->addWidget(gt);
        auto* div { new QFrame }; div->setObjectName("Divider"); div->setFixedHeight(1); fl->addWidget(div);
        for (const auto& [label, value] : items) {
            auto* row { new QHBoxLayout };
            auto* lbl { new QLabel(label) }; lbl->setObjectName("SettingsLabel");
            auto* val { new QLabel(value) }; val->setStyleSheet("color: #64748b; font-size: 12px;");
            row->addWidget(lbl); row->addStretch(); row->addWidget(val);
            fl->addLayout(row);
        }
        layout->addWidget(frame);
    };

    make_group("RUNTIME", {
                           { "Docker socket", "/var/run/docker.sock" },
                           { "API version",   "v1.45"                },
                           { "Runtime",       "runc"                 },
                           });
    make_group("APPLICATION", {
                               { "Version",    "1.0.0"            },
                               { "Build",      "quiver-gui / C++" },
                               { "Qt version", "6.x"              },
                               });
    layout->addStretch();
}
SettingsPage::~SettingsPage() = default;

}
