#include "include/ContainerDetailsPage.h"
#include "include/Backend.h"
#include "include/Components.h"
#include "include/FlowLayout.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QPair>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QMap>
#include <QStringList>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace Quiver {

struct ContainerDetailsPage::Impl {
    QString container_id_{};
    QVBoxLayout* content_layout_{};
    QLabel* title_lbl_{};
    QPushButton* refresh_btn_{};
    
    QWidget* overlay_{};
    QLabel* loading_text_{};
    
    // Store parsed data: Section Name -> (Key -> Value or List)
    QMap<QString, QMap<QString, QString>> sections_kv;
    QMap<QString, QStringList> sections_list; // For simple lists or "  • item"

    void clear_layout(QLayout* layout) {
        if (!layout) return;
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            if (item->layout()) {
                clear_layout(item->layout());
            }
            delete item;
        }
    }

    void clear_content() {
        clear_layout(content_layout_);
        sections_kv.clear();
        sections_list.clear();
    }
};

ContainerDetailsPage::ContainerDetailsPage(QWidget* parent)
    : QWidget(parent), pimpl_{std::make_unique<Impl>()}
{
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(20);

    // Header Top Row
    auto* header = new QHBoxLayout;
    
    auto* back_btn = new QPushButton("← Back to Containers");
    back_btn->setStyleSheet(
        "QPushButton { background: transparent; color: #a1a1aa; border: none; font-size: 14px; font-weight: bold; text-align: left; }"
        "QPushButton:hover { color: #f97316; }"
    );
    back_btn->setFixedSize(200, 36);
    back_btn->setCursor(Qt::PointingHandCursor);
    connect(back_btn, &QPushButton::clicked, this, [this](){ emit back_requested(); });
    
    pimpl_->refresh_btn_ = new QPushButton("Refresh");
    pimpl_->refresh_btn_->setStyleSheet(
        "QPushButton { background: #f97316; color: #ffffff; border: none; border-radius: 6px; font-weight: bold; font-size: 14px; }"
        "QPushButton:hover { background: #ea580c; }"
    );
    pimpl_->refresh_btn_->setFixedSize(120, 36);
    pimpl_->refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(pimpl_->refresh_btn_, &QPushButton::clicked, this, &ContainerDetailsPage::refresh);

    header->addWidget(back_btn);
    header->addStretch();
    header->addWidget(pimpl_->refresh_btn_);
    root_layout->addLayout(header);

    // Header Second Row: Title
    auto* title_layout = new QHBoxLayout;
    pimpl_->title_lbl_ = new QLabel("Container Specification");
    pimpl_->title_lbl_->setStyleSheet("color: #ffffff; font-size: 24px; font-weight: bold; margin-top: 10px; margin-bottom: 5px;");
    title_layout->addWidget(pimpl_->title_lbl_);
    title_layout->addStretch();
    root_layout->addLayout(title_layout);

    // Create the overlay for loading
    pimpl_->overlay_ = new QWidget(this);
    pimpl_->overlay_->setFixedSize(160, 40);
    pimpl_->overlay_->setStyleSheet("background-color: #27272A; border: 1px solid #3F3F46; border-radius: 20px;");
    pimpl_->overlay_->hide(); // Hide by default
    
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
    timer->start(100);

    // Scroll Area for content
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; } QWidget#DetailsContent { background: transparent; }");
    
    auto* content_widget = new QWidget;
    content_widget->setObjectName("DetailsContent");
    pimpl_->content_layout_ = new QVBoxLayout(content_widget);
    pimpl_->content_layout_->setContentsMargins(0, 0, 0, 0);
    pimpl_->content_layout_->setSpacing(20);
    pimpl_->content_layout_->setAlignment(Qt::AlignTop);
    
    scroll->setWidget(content_widget);
    root_layout->addWidget(scroll);
}

ContainerDetailsPage::~ContainerDetailsPage() = default;

auto ContainerDetailsPage::set_container_id(const QString& id) -> void {
    if (pimpl_->container_id_ != id) {
        pimpl_->container_id_ = id;
    }
    refresh();
}

static QFrame* create_card() {
    auto* card = new QFrame;
    card->setObjectName("Card");
    card->setStyleSheet(
        "QFrame#Card {"
        "    background: #18181b;"
        "    border: 1px solid #27272a;"
        "    border-radius: 8px;"
        "}"
    );
    return card;
}

static QLabel* create_badge(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(
        "background: #27272a;"
        "color: #e4e4e7;"
        "border-radius: 4px;"
        "padding: 4px 8px;"
        "font-size: 12px;"
    );
    return lbl;
}

auto ContainerDetailsPage::refresh() -> void {
    pimpl_->title_lbl_->setText("Loading Specification...");
    pimpl_->clear_content();
    
    auto* watcher = new QFutureWatcher<QPair<QString, QString>>(this);
    pimpl_->overlay_->show();
    pimpl_->overlay_->raise();
    connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher]() {
        pimpl_->overlay_->hide();
        QString output = watcher->result().first;
        QString top_output = watcher->result().second;
        watcher->deleteLater();
        
        pimpl_->title_lbl_->setText("Container Specification");
        
        if (output.isEmpty()) {
            auto* lbl = new QLabel("Failed to fetch container details.");
            lbl->setStyleSheet("color: #ef4444; font-size: 16px;");
            pimpl_->content_layout_->addWidget(lbl);
            return;
        }

        // --- Basic Parser ---
        QStringList lines = output.split('\n');
        QString current_section;
        QString current_key; // For lists under a key, like Capabilities -> Bounding
        
        for (int i = 0; i < lines.size(); ++i) {
            QString line = lines[i];
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            
            // Check if it's a section header (next line is ======)
            if (i + 1 < lines.size() && lines[i+1].startsWith("======")) {
                current_section = trimmed;
                i++; // skip the ====== line
                continue;
            }
            
            if (current_section.isEmpty()) continue;
            
            if (line.startsWith("  • ")) {
                // It's a bullet point
                QString val = line.mid(4).trimmed();
                if (!current_key.isEmpty()) {
                    pimpl_->sections_kv[current_section][current_key] += val + "\n";
                } else {
                    pimpl_->sections_list[current_section] << val;
                }
            } 
            else if (line.startsWith("  ") && current_section == "Resource Limits") {
                // e.g. "  nofile           soft=524288 hard=524288"
                // Let's just treat it as a kv pair by splitting on first spaces
                line = line.trimmed();
                int space_idx = line.indexOf(' ');
                if (space_idx != -1) {
                    QString k = line.left(space_idx).trimmed();
                    QString v = line.mid(space_idx).trimmed();
                    pimpl_->sections_kv[current_section][k] = v;
                }
            }
            else if (line.startsWith("  ") && !line.contains(":")) {
                 pimpl_->sections_list[current_section] << line.trimmed();
            }
            else if (line.contains(" : ")) {
                int col_idx = line.indexOf(" : ");
                QString k = line.left(col_idx).trimmed();
                QString v = line.mid(col_idx + 3).trimmed();
                if (v.isEmpty()) {
                    current_key = k; // A key with an empty value, likely preceding a bullet list
                    pimpl_->sections_kv[current_section][k] = ""; // Initialize empty string for list
                } else {
                    pimpl_->sections_kv[current_section][k] = v;
                    current_key = ""; // Reset
                }
            } else if (line.contains(":")) {
                int col_idx = line.indexOf(":");
                QString k = line.left(col_idx).trimmed();
                QString v = line.mid(col_idx + 1).trimmed();
                if (v.isEmpty()) {
                    current_key = k;
                    pimpl_->sections_kv[current_section][k] = "";
                } else {
                    pimpl_->sections_kv[current_section][k] = v;
                    current_key = "";
                }
            }
        }
        // --- End Parser ---
        
        // --- Build UI ---
        auto create_kv_layout = [](const QString& k, const QString& v) -> QWidget* {
            auto* w = new QWidget;
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(0,0,0,0);
            
            auto* kl = new QLabel(k);
            kl->setStyleSheet("color: #a1a1aa; font-size: 14px;");
            kl->setFixedWidth(200);
            
            auto* vl = new QLabel(v);
            vl->setStyleSheet("color: #e4e4e7; font-size: 14px; font-weight: bold;");
            vl->setWordWrap(true);
            
            h->addWidget(kl);
            h->addWidget(vl);
            h->addStretch();
            return w;
        };

        auto build_card = [&](const QString& sec_name, int fixed_height) -> QWidget* {
            bool has_kv = pimpl_->sections_kv.contains(sec_name);
            bool has_list = pimpl_->sections_list.contains(sec_name);
            if (!has_kv && !has_list) return nullptr;
            
            auto* card = create_card();
            if (fixed_height > 0) card->setMinimumHeight(fixed_height);
            
            auto* card_l = new QVBoxLayout(card);
            card_l->setContentsMargins(0, 0, 0, 0);
            
            auto* sec_lbl = new QLabel(sec_name);
            sec_lbl->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: bold; margin: 20px 20px 10px 20px;");
            card_l->addWidget(sec_lbl);
            
            auto* scroll = new QScrollArea;
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->setStyleSheet("QScrollArea { background: transparent; border: none; } QWidget#ScrollInner { background: transparent; }");
            
            auto* inner_w = new QWidget;
            inner_w->setObjectName("ScrollInner");
            auto* inner_l = new QVBoxLayout(inner_w);
            inner_l->setContentsMargins(20, 0, 20, 20);
            inner_l->setSpacing(10);
            
            if (has_kv) {
                QMap<QString, QString> kvs = pimpl_->sections_kv[sec_name];
                if (sec_name == "Capabilities") {
                    for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
                        inner_l->addWidget(new QLabel("<span style='color:#a1a1aa; font-weight:bold;'>" + it.key() + "</span>"));
                        auto* flow = new FlowLayout(-1, 8, 8);
                        for (const auto& item : it.value().split('\n', Qt::SkipEmptyParts)) flow->addWidget(create_badge(item));
                        inner_l->addLayout(flow);
                    }
                } else if (sec_name == "Resource Limits") {
                    auto* gauge_layout = new QHBoxLayout;
                    gauge_layout->addStretch();
                    for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
                        QRegularExpressionMatch match = QRegularExpression("soft=(\\d+).*hard=(\\d+)").match(it.value());
                        double soft = match.hasMatch() ? match.captured(1).toDouble() : 0;
                        double hard = match.hasMatch() ? match.captured(2).toDouble() : 0;
                        gauge_layout->addWidget(new CircularGauge(it.key(), soft, hard));
                    }
                    gauge_layout->addStretch();
                    inner_l->addLayout(gauge_layout);
                } else {
                    for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
                        if (it.value().contains("\n") || it.value().length() > 60) {
                            inner_l->addWidget(new QLabel("<span style='color:#a1a1aa; font-weight:bold;'>" + it.key() + "</span>"));
                            auto* vl = new QLabel(it.value());
                            vl->setStyleSheet("color: #e4e4e7; font-size: 13px; font-family: monospace; background: #27272a; padding: 10px; border-radius: 4px;");
                            inner_l->addWidget(vl);
                        } else {
                            inner_l->addWidget(create_kv_layout(it.key(), it.value()));
                        }
                    }
                }
            }
            if (has_list) {
                auto* flow = new FlowLayout(-1, 8, 8);
                for (const auto& item : pimpl_->sections_list[sec_name]) flow->addWidget(create_badge(item));
                inner_l->addLayout(flow);
            }
            inner_l->addStretch();
            scroll->setWidget(inner_w);
            card_l->addWidget(scroll);
            return card;
        };

        QStringList rendered;

        auto build_row = [&](const QStringList& sections, int height, const QList<int>& stretches = {}) {
            auto* row = new QHBoxLayout;
            row->setSpacing(20);
            for (int i = 0; i < sections.size(); ++i) {
                const auto& sec = sections[i];
                auto* card = build_card(sec, height);
                if (card) {
                    int stretch = (i < stretches.size()) ? stretches[i] : 1;
                    row->addWidget(card, stretch);
                    rendered << sec;
                }
            }
            if (row->count() > 0) pimpl_->content_layout_->addLayout(row);
            else delete row;
        };

        build_row({"Capabilities"}, 350);
        build_row({"General", "Processes", "State", "Process"}, 430, {3, 2, 2, 2});
        build_row({"Resource Limits", "Network", "Root Filesystem"}, 250);
        build_row({"Scheduler", "Seccomp", "Terminal", "Security"}, 380);
        build_row({"MaskedPaths", "Masked Paths", "Namespaces"}, 150);
        
        // --- Render Readonly Paths and Running Processes ---
        auto* rop_rp_row = new QHBoxLayout;
        rop_rp_row->setSpacing(20);
        
        auto* rop_card = build_card("Read Only Paths", 0);
        if (!rop_card) rop_card = build_card("Read only paths", 0);
        if (!rop_card) rop_card = build_card("Readonly Paths", 0);
        if (!rop_card) rop_card = build_card("Readonly paths", 0);
        
        if (rop_card) {
            rop_card->setMaximumHeight(320);
            rop_rp_row->addWidget(rop_card, 3);
            rendered << "Readonly paths" << "Readonly Paths" << "Read Only Paths" << "Read only paths";
        }


        if (!top_output.isEmpty()) {
            auto* top_card = new QFrame;
            top_card->setObjectName("Card");
            top_card->setMaximumHeight(320);
            top_card->setStyleSheet(
                "QFrame#Card {"
                "    background: #18181b;"
                "    border: 1px solid #27272a;"
                "    border-radius: 8px;"
                "}"
            );
            auto* top_layout = new QVBoxLayout(top_card);
            top_layout->setContentsMargins(20, 20, 20, 20);
            
            auto* top_title = new QLabel("Running Processes (top)");
            top_title->setStyleSheet("color: #e4e4e7; font-size: 16px; font-weight: bold;");
            top_layout->addWidget(top_title);

            QStringList top_lines = top_output.split('\n', Qt::SkipEmptyParts);
            if (top_lines.size() > 1) { 
                QTableWidget* table = new QTableWidget(top_lines.size() - 1, 4);
                table->setMinimumHeight(200);
                table->horizontalHeader()->setStretchLastSection(true);
                table->setWordWrap(true);
                table->setTextElideMode(Qt::ElideNone);
                table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
                table->verticalHeader()->setVisible(false);
                table->setEditTriggers(QAbstractItemView::NoEditTriggers);
                table->setSelectionBehavior(QAbstractItemView::SelectRows);
                table->setSelectionMode(QAbstractItemView::SingleSelection);
                table->setShowGrid(false);
                table->setFocusPolicy(Qt::NoFocus);
                
                table->setStyleSheet(
                    "QTableWidget {"
                    "    background-color: transparent;"
                    "    color: #e4e4e7;"
                    "    border: none;"
                    "}"
                    "QHeaderView::section {"
                    "    background-color: #27272a;"
                    "    color: #a1a1aa;"
                    "    font-size: 12px;"
                    "    font-weight: bold;"
                    "    padding: 8px;"
                    "    border: none;"
                    "    border-bottom: 1px solid #3f3f46;"
                    "}"
                    "QTableWidget::item {"
                    "    padding: 8px;"
                    "    border-bottom: 1px solid #27272a;"
                    "}"
                    "QTableWidget::item:selected {"
                    "    background-color: #27272a;"
                    "}"
                    "QScrollBar:vertical {"
                    "    border: none;"
                    "    background: transparent;"
                    "    width: 6px;"
                    "    margin: 0px;"
                    "}"
                    "QScrollBar::handle:vertical {"
                    "    background: #3f3f46;"
                    "    min-height: 20px;"
                    "    border-radius: 3px;"
                    "}"
                    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                    "    height: 0px;"
                    "}"
                    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
                    "    background: none;"
                    "}"
                );

                QStringList headers = {"UID", "PID", "STAT", "CMD"};
                table->setHorizontalHeaderLabels(headers);
                
                for (int r = 1; r < top_lines.size(); ++r) {
                    QStringList parts = top_lines[r].split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                    for (int c = 0; c < 4 && c < parts.size(); ++c) {
                        QString val = parts[c];
                        if (c == 3 && parts.size() > 4) { 
                            val = top_lines[r].mid(top_lines[r].indexOf(parts[c])).trimmed();
                        }
                        QTableWidgetItem* item = new QTableWidgetItem(val);
                        table->setItem(r - 1, c, item);
                    }
                }
                
                table->resizeColumnsToContents();
                top_layout->addWidget(table);
            } else {
                auto* empty = new QLabel("No running processes.");
                empty->setStyleSheet("color: #a1a1aa; font-style: italic;");
                top_layout->addWidget(empty);
            }
            
            rop_rp_row->addWidget(top_card, 7);
        }

        
        if (rop_rp_row->count() > 0) {
            pimpl_->content_layout_->addLayout(rop_rp_row);
        } else {
            delete rop_rp_row;
        }

        auto* grid = new QGridLayout;
        grid->setSpacing(20);
        int grid_row = 0, grid_col = 0;
        
        QStringList all_sections = pimpl_->sections_kv.keys() + pimpl_->sections_list.keys();
        all_sections.removeDuplicates();
        for (const auto& sec : all_sections) {
            if (rendered.contains(sec)) continue;
            auto* card = build_card(sec, 0);
            if (!card) continue;
            
            bool is_wide = (sec == "Arguments" || sec == "Environment");
            if (is_wide) {
                if (grid_col > 0) { grid_row++; grid_col = 0; }
                grid->addWidget(card, grid_row, 0, 1, 2);
                grid_row++;
            } else {
                grid->addWidget(card, grid_row, grid_col);
                grid_col++;
                if (grid_col > 1) { grid_col = 0; grid_row++; }
            }
        }
        pimpl_->content_layout_->addLayout(grid);

        pimpl_->content_layout_->addStretch();
    });
    
    watcher->setFuture(QtConcurrent::run([id = pimpl_->container_id_]() {
        QString inspect = Backend::get_instance().get_container_inspect(id);
        QString top = Backend::get_instance().get_container_top(id);
        return qMakePair(inspect, top);
    }));
}

void ContainerDetailsPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (pimpl_->overlay_) {
        pimpl_->overlay_->move(width() / 2 - pimpl_->overlay_->width() / 2,
                               height() / 2 - pimpl_->overlay_->height() / 2);
    }
}

} // namespace Quiver
