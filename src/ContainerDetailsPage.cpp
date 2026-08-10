#include "include/ContainerDetailsPage.h"
#include "include/Backend.h"
#include "include/Components.h"
#include "include/FlowLayout.h"
#include <QVBoxLayout>
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
    
    // Store parsed data: Section Name -> (Key -> Value or List)
    QMap<QString, QMap<QString, QString>> sections_kv;
    QMap<QString, QStringList> sections_list; // For simple lists or "  • item"

    void clear_content() {
        QLayoutItem* item;
        while ((item = content_layout_->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            if (item->layout()) {
                QLayoutItem* child;
                while ((child = item->layout()->takeAt(0)) != nullptr) {
                    if (child->widget()) child->widget()->deleteLater();
                    delete child;
                }
                delete item->layout();
            }
            delete item;
        }
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

    // Header
    auto* header = new QHBoxLayout;
    auto* back_btn = new QPushButton("← Back to Containers");
    back_btn->setObjectName("SecondaryButton");
    back_btn->setFixedSize(200, 36);
    back_btn->setCursor(Qt::PointingHandCursor);
    connect(back_btn, &QPushButton::clicked, this, [this](){ emit back_requested(); });
    
    pimpl_->title_lbl_ = new QLabel("Container Details");
    pimpl_->title_lbl_->setObjectName("PageTitle");
    
    pimpl_->refresh_btn_ = new QPushButton("Refresh");
    pimpl_->refresh_btn_->setObjectName("SecondaryButton");
    pimpl_->refresh_btn_->setFixedSize(120, 36);
    pimpl_->refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(pimpl_->refresh_btn_, &QPushButton::clicked, this, &ContainerDetailsPage::refresh);

    header->addWidget(back_btn);
    header->addSpacing(20);
    header->addWidget(pimpl_->title_lbl_);
    header->addStretch();
    header->addWidget(pimpl_->refresh_btn_);
    root_layout->addLayout(header);

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
    pimpl_->title_lbl_->setText("Loading " + pimpl_->container_id_.left(8) + "...");
    pimpl_->clear_content();
    
    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        QString output = watcher->result();
        watcher->deleteLater();
        
        pimpl_->title_lbl_->setText("Container: " + pimpl_->container_id_.left(12));
        
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

        // Layout sections in a FlowLayout or Grid? A 2-column layout for cards is good.
        // We'll just use a QVBoxLayout for simplicity, or we can use QGridLayout.
        auto* grid = new QGridLayout;
        grid->setSpacing(20);
        int row = 0, col = 0;

        for (auto sec_it = pimpl_->sections_kv.constBegin(); sec_it != pimpl_->sections_kv.constEnd(); ++sec_it) {
            QString sec_name = sec_it.key();
            QMap<QString, QString> kvs = sec_it.value();
            
            auto* card = create_card();
            auto* card_l = new QVBoxLayout(card);
            card_l->setContentsMargins(20, 20, 20, 20);
            card_l->setSpacing(10);
            
            auto* sec_lbl = new QLabel(sec_name);
            sec_lbl->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: bold; margin-bottom: 10px;");
            card_l->addWidget(sec_lbl);
            
            if (sec_name == "Capabilities") {
                // Capabilities are long lists. Use tags.
                for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
                    card_l->addWidget(new QLabel("<span style='color:#a1a1aa; font-weight:bold;'>" + it.key() + "</span>"));
                    auto* flow = new FlowLayout;
                    QStringList items = it.value().split('\n', Qt::SkipEmptyParts);
                    for (const auto& item : items) {
                        flow->addWidget(create_badge(item));
                    }
                    card_l->addLayout(flow);
                }
            } else if (sec_name == "Resource Limits") {
                // Use CircularGauge
                auto* gauge_layout = new FlowLayout;
                for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
                    // value is like "soft=524288 hard=524288"
                    QString val = it.value();
                    QRegularExpression re("soft=(\\d+).*hard=(\\d+)");
                    QRegularExpressionMatch match = re.match(val);
                    double soft = 0, hard = 0;
                    if (match.hasMatch()) {
                        soft = match.captured(1).toDouble();
                        hard = match.captured(2).toDouble();
                    }
                    auto* gauge = new CircularGauge(it.key(), soft, hard);
                    gauge_layout->addWidget(gauge);
                }
                card_l->addLayout(gauge_layout);
            } else {
                for (auto it = kvs.constBegin(); it != kvs.constEnd(); ++it) {
                    if (it.value().contains("\n")) {
                        // Multi-line value (like Arguments or Environment)
                        card_l->addWidget(new QLabel("<span style='color:#a1a1aa; font-weight:bold;'>" + it.key() + "</span>"));
                        auto* vl = new QLabel(it.value());
                        vl->setStyleSheet("color: #e4e4e7; font-size: 13px; font-family: monospace; background: #27272a; padding: 10px; border-radius: 4px;");
                        card_l->addWidget(vl);
                    } else {
                        card_l->addWidget(create_kv_layout(it.key(), it.value()));
                    }
                }
            }
            
            // Check if there's a simple list for this section
            if (pimpl_->sections_list.contains(sec_name)) {
                auto* flow = new FlowLayout;
                for (const auto& item : pimpl_->sections_list[sec_name]) {
                    flow->addWidget(create_badge(item));
                }
                card_l->addLayout(flow);
            }

            card_l->addStretch();
            grid->addWidget(card, row, col);
            col++;
            if (col > 1) { col = 0; row++; }
        }
        
        // Add sections that ONLY have simple lists
        for (auto sec_it = pimpl_->sections_list.constBegin(); sec_it != pimpl_->sections_list.constEnd(); ++sec_it) {
            if (pimpl_->sections_kv.contains(sec_it.key())) continue; // Already handled
            
            auto* card = create_card();
            auto* card_l = new QVBoxLayout(card);
            card_l->setContentsMargins(20, 20, 20, 20);
            
            auto* sec_lbl = new QLabel(sec_it.key());
            sec_lbl->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: bold; margin-bottom: 10px;");
            card_l->addWidget(sec_lbl);
            
            auto* flow = new FlowLayout;
            for (const auto& item : sec_it.value()) {
                flow->addWidget(create_badge(item));
            }
            card_l->addLayout(flow);
            card_l->addStretch();
            grid->addWidget(card, row, col);
            col++;
            if (col > 1) { col = 0; row++; }
        }

        pimpl_->content_layout_->addLayout(grid);
        pimpl_->content_layout_->addStretch();
    });
    
    watcher->setFuture(QtConcurrent::run([id = pimpl_->container_id_]() {
        return Backend::get_instance().get_container_inspect(id);
    }));
}

} // namespace Quiver
