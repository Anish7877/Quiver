#include <QTextEdit>
#include "include/Components.h"
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QProgressBar>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QButtonGroup>
#include <QAction>
#include <QCursor>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QScrollBar>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QIntValidator>
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QRandomGenerator>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QPainterPath>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>
#include <QClipboard>
#include <QPointer>
#include <QTimer>
#include <QMenu>
#include <QJsonObject>
#include <QJsonArray>
#include "include/Backend.h"
#include "include/AuthManager.h"

namespace Quiver {

static void add_item_with_delete_btn(QListWidget* list, const QString& text) {
    if (!list->findItems(text, Qt::MatchExactly).isEmpty()) return;
    auto* item = new QListWidgetItem(text, list);
    item->setSizeHint(QSize(0, 32));
    item->setForeground(QBrush(Qt::transparent));
    
    auto* w = new QWidget;
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(5, 0, 5, 0);
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet("color: #fafafa; font-size: 13px;");
    auto* btn = new QPushButton("×");
    btn->setFixedSize(24, 24);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet("QPushButton { color: #f87171; background: transparent; border: none; font-weight: bold; font-size: 18px; } QPushButton:hover { color: #ef4444; }");
    QObject::connect(btn, &QPushButton::clicked, [list, item]() {
        delete list->takeItem(list->row(item));
    });
    l->addWidget(lbl);
    l->addStretch();
    l->addWidget(btn);
    list->setItemWidget(item, w);
}


struct ToggleSwitch::Impl {};
ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QCheckBox(parent), pimpl_{std::make_unique<Impl>()}
{
    setCursor(Qt::PointingHandCursor);
    setFixedSize(50, 26);
}
ToggleSwitch::~ToggleSwitch() = default;


auto ToggleSwitch::hitButton(const QPoint& pos) const -> bool {
    return rect().contains(pos);
}
auto ToggleSwitch::paintEvent(QPaintEvent*) -> void {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    bool on { isChecked() };
    QColor bg { on ? QColor{"#F97316"} : QColor{"#3F3F46"} };
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(0, 0, width(), height(), 13, 13);
    p.setBrush(Qt::white);
    int x { on ? width() - 23 : 3 };
    p.drawEllipse(x, 3, 20, 20);
}


struct ActivityGraph::Impl { std::vector<double> data_ {}; };
ActivityGraph::ActivityGraph(QWidget* parent)
    : QWidget(parent), pimpl_{std::make_unique<Impl>()} {}
ActivityGraph::~ActivityGraph() = default;
auto ActivityGraph::paintEvent(QPaintEvent*) -> void {}

struct StatCard::Impl {
    QLabel* value_label_ {};
};

StatCard::StatCard(const QString& title, const QString& value,
                   const QString& color, QWidget* parent)
    : QFrame(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("StatPanel");
    setFixedHeight(75);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true); 

  
    QString theme = "orange"; 
    if (color == "#4ade80" || color.contains("4ade80", Qt::CaseInsensitive)) theme = "green";
    else if (color == "#fb7185" || color.contains("fb7185", Qt::CaseInsensitive)) theme = "red";
    else if (color == "#ffffff" || color == "white") theme = "white"; 
    setProperty("statTheme", theme); 

   
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 30)); 
    setGraphicsEffect(shadow);

    auto* layout { new QVBoxLayout(this) };
    layout->setContentsMargins(15, 12, 15, 12);
    layout->setSpacing(2);

    auto* t { new QLabel(title) };
    t->setObjectName("StatTitle"); 

    auto* v { new QLabel(value) };
    v->setObjectName("StatValue");
    pimpl_->value_label_ = v;

    layout->addWidget(t);
    layout->addWidget(v);
    layout->addStretch();
}

StatCard::~StatCard() = default;

auto StatCard::set_value(const QString& val) -> void {
    if (pimpl_->value_label_) {
        pimpl_->value_label_->setText(val);
    }
}


struct ResourceTable::Impl {};

ResourceTable::ResourceTable(const QStringList& headers, QWidget* parent)
    : QTableWidget(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("ResourceTable");
    setColumnCount(static_cast<int>(headers.size()));
    setHorizontalHeaderLabels(headers);

    setShowGrid(false);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFocusPolicy(Qt::NoFocus);

    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(56); 

    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

ResourceTable::~ResourceTable() = default;

auto ResourceTable::add_status_badge(int row, int col, const QString& status) -> void {

    bool is_positive {
        status == "running"   ||
        status == "active"    ||
        status == "available" ||
        status == "mounted"   ||
        status == "assigned"
    };

    QString bg_color  { is_positive ? "rgba(74,222,128,0.15)"  : "rgba(251,113,133,0.15)" };
    QString txt_color { is_positive ? "#4ade80"                : "#fb7185" };
    QString dot_color { is_positive ? "#4ade80"                : "#fb7185" };

    auto* badge_widget { new QWidget };
    badge_widget->setObjectName("TableBadgeCell");
    auto* h { new QHBoxLayout(badge_widget) };
    h->setContentsMargins(0, 0, 0, 0);
    h->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* badge { new QFrame };
    badge->setFixedSize(90, 26);
    auto* bl { new QHBoxLayout(badge) };
    bl->setContentsMargins(8, 0, 8, 0);
    bl->setSpacing(5);

    auto* dot { new QLabel };
    dot->setFixedSize(7, 7);
    dot->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(dot_color));
    auto* lbl { new QLabel(status.toUpper()) };
    lbl->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;").arg(txt_color));

    badge->setStyleSheet(QString(
        "QFrame { background: %1; border: 1px solid %2; border-radius: 5px; }"
    ).arg(bg_color, dot_color));

    bl->addWidget(dot);
    bl->addWidget(lbl);
    h->addSpacing(8);
    h->addWidget(badge);

    setCellWidget(row, col, badge_widget);
}


static auto make_table_item(const QString& text) -> QTableWidgetItem* {
    auto* item { new QTableWidgetItem(text) };
    item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    return item;
}

struct DeleteDialog::Impl {};
DeleteDialog::DeleteDialog(const QString& container_name, QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    setWindowTitle("Delete Container");
    setFixedSize(400, 200);
    setObjectName("CreateDialog");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QLineEdit:read-only { background: #1f1f22; color: #a1a1aa; }"
    );


    auto* layout { new QVBoxLayout(this) };
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(15);

    auto* title { new QLabel("Delete Container") };
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto* msg { new QLabel(
        "Are you sure you want to delete <b>" + container_name +
        "</b>?<br>This action cannot be undone.") };
    msg->setStyleSheet("color: #a1a1aa; font-size: 14px;");
    msg->setWordWrap(true);
    layout->addWidget(msg);
    layout->addStretch();

    auto* btns { new QHBoxLayout };
    auto* cancel { new QPushButton("Cancel") };
    cancel->setObjectName("SecondaryBtn");
    cancel->setCursor(Qt::PointingHandCursor);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto* del_btn { new QPushButton("Delete") };
    del_btn->setObjectName("DangerButton");
    del_btn->setCursor(Qt::PointingHandCursor);
    connect(del_btn, &QPushButton::clicked, this, &QDialog::accept);
    btns->addStretch();
    btns->addWidget(cancel);
    btns->addWidget(del_btn);
    layout->addLayout(btns);
}
DeleteDialog::~DeleteDialog() = default;






struct BuildProgressDialog::Impl {
    QProgressBar* progress_bar_ {};
    QLabel*       status_label_ {};
    QTextEdit*    log_view_     {};
    QProcess*     process_      {};
};

BuildProgressDialog::BuildProgressDialog(const QStringList& build_args, QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("CreateDialog");
    setWindowTitle("Building Image");
    setFixedSize(550, 400);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QLineEdit:read-only { background: #1f1f22; color: #a1a1aa; }"
    );


    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(15);

    auto* title = new QLabel("Building Image...");
    title->setObjectName("PageTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    pimpl_->progress_bar_ = new QProgressBar;
    pimpl_->progress_bar_->setRange(0, 100);
    pimpl_->progress_bar_->setValue(0);
    pimpl_->progress_bar_->setTextVisible(true);
    pimpl_->progress_bar_->setAlignment(Qt::AlignCenter);
    pimpl_->progress_bar_->setStyleSheet(
        "QProgressBar { border: 1px solid #3f3f46; border-radius: 6px; background: transparent; color: white; }"
        "QProgressBar::chunk { background-color: #f97316; border-radius: 5px; }"
    );
    pimpl_->progress_bar_->setFixedHeight(24);
    layout->addWidget(pimpl_->progress_bar_);

    pimpl_->status_label_ = new QLabel("Starting build process...");
    pimpl_->status_label_->setStyleSheet("color: #a1a1aa; font-size: 13px;");
    layout->addWidget(pimpl_->status_label_);
    
    pimpl_->log_view_ = new QTextEdit;
    pimpl_->log_view_->setReadOnly(true);
    pimpl_->log_view_->setStyleSheet("background: #18181b; color: #d4d4d8; border: 1px solid #3f3f46; border-radius: 6px; font-family: monospace; font-size: 11px;");
    layout->addWidget(pimpl_->log_view_);

    auto* btns = new QHBoxLayout;
    btns->addStretch();
    auto* cancel_btn = new QPushButton("Cancel / Close");
    cancel_btn->setObjectName("SecondaryBtn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    connect(cancel_btn, &QPushButton::clicked, this, [this]() {
        if (pimpl_->process_ && pimpl_->process_->state() == QProcess::Running) {
            pimpl_->process_->terminate();
        }
        reject();
    });
    btns->addWidget(cancel_btn);
    layout->addLayout(btns);

    pimpl_->process_ = new QProcess(this);
    
    auto parse_output = [this](const QString& output) {
        QString clean = output.trimmed();
        if (clean.isEmpty()) return;
        
        QRegularExpression re(R"(\[([a-zA-Z0-9_-]+)\s+(\d+)/(\d+)\])");
        auto match = re.match(clean);
        if (match.hasMatch()) {
            int current = match.captured(2).toInt();
            int total = match.captured(3).toInt();
            if (total > 0) {
                int percentage = (current * 100) / total;
                pimpl_->progress_bar_->setValue(percentage);
            }
            pimpl_->status_label_->setText(QString("Running %1 step %2 of %3...").arg(match.captured(1)).arg(current).arg(total));
        } else if (clean.contains("FINISHED")) {
            pimpl_->progress_bar_->setValue(100);
            pimpl_->status_label_->setText("Build finished successfully!");
        } else if (clean.contains("exporting")) {
            pimpl_->status_label_->setText("Exporting image...");
        }
    };
    
    connect(pimpl_->process_, &QProcess::readyReadStandardOutput, this, [this, parse_output]() {
        QString output = pimpl_->process_->readAllStandardOutput();
        pimpl_->log_view_->append(output.trimmed());
        parse_output(output);
    });

    connect(pimpl_->process_, &QProcess::readyReadStandardError, this, [this, parse_output]() {
        QString output = pimpl_->process_->readAllStandardError();
        pimpl_->log_view_->append("<span style=\"color: #f87171;\">" + output.trimmed().replace("\n", "<br>") + "</span>");
        parse_output(output);
    });

    connect(pimpl_->process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
            pimpl_->progress_bar_->setValue(100);
            pimpl_->status_label_->setText("Build complete!");
            // Accept the dialog automatically after a successful build
            QTimer::singleShot(2000, this, &QDialog::accept);
        } else {
            pimpl_->status_label_->setText("Build failed! Check logs.");
        }
    });

    QString cli_path = Quiver::Backend::get_instance().get_cli_path();
    pimpl_->process_->start(cli_path, build_args);
}

BuildProgressDialog::~BuildProgressDialog() {
    if (pimpl_->process_ && pimpl_->process_->state() == QProcess::Running) {
        pimpl_->process_->terminate();
        pimpl_->process_->waitForFinished(1000);
    }
}


#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QFileDialog>
#include <QStackedWidget>

struct PullImageDialog::Impl {
    QStackedWidget* stack_{};
    
    // Pull section
    QLineEdit* pull_name_{};
    QLineEdit* pull_tag_{};
    
    // Load section
    QLineEdit* load_name_{};
    QLineEdit* load_tag_{};
    QLineEdit* load_path_{};
    
    // Controls
    QPushButton* pull_btn_{};
    QPushButton* load_btn_{};
    QPushButton* confirm_btn_{};
    
    QWidget* overlay_{};
    QLabel* loading_text_{};
    
    void switch_to(int index) {
        if (stack_->currentIndex() == index) return;
        
        auto* current_widget = stack_->currentWidget();
        auto* next_widget = stack_->widget(index);
        
        auto* current_effect = new QGraphicsOpacityEffect(current_widget);
        current_widget->setGraphicsEffect(current_effect);
        auto* fade_out = new QPropertyAnimation(current_effect, "opacity");
        fade_out->setDuration(150);
        fade_out->setStartValue(1.0);
        fade_out->setEndValue(0.0);
        
        QObject::connect(fade_out, &QPropertyAnimation::finished, [=]() {
            stack_->setCurrentIndex(index);
            
            auto* next_effect = new QGraphicsOpacityEffect(next_widget);
            next_widget->setGraphicsEffect(next_effect);
            auto* fade_in = new QPropertyAnimation(next_effect, "opacity");
            fade_in->setDuration(150);
            fade_in->setStartValue(0.0);
            fade_in->setEndValue(1.0);
            fade_in->start(QAbstractAnimation::DeleteWhenStopped);
            
            QObject::connect(fade_in, &QPropertyAnimation::finished, [=]() {
                next_widget->setGraphicsEffect(nullptr);
                current_widget->setGraphicsEffect(nullptr);
            });
        });
        
        fade_out->start(QAbstractAnimation::DeleteWhenStopped);
    }
};

PullImageDialog::PullImageDialog(QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("CreateDialog");
    setFixedSize(450, 320); // slightly taller to accommodate tabs and load fields
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QLineEdit:read-only { background: #1f1f22; color: #a1a1aa; }"
    );

    
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(28, 28, 28, 28);
    main_layout->setSpacing(20);
    
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("Image Options");
    title->setObjectName("PageTitle");
    header->addWidget(title);
    header->addStretch();
    main_layout->addLayout(header);
    
    // Section Switcher
    auto* tabs_layout = new QHBoxLayout;
    tabs_layout->setSpacing(10);
    
    pimpl_->pull_btn_ = new QPushButton("Pull Image");
    pimpl_->pull_btn_->setCursor(Qt::PointingHandCursor);
    pimpl_->pull_btn_->setFixedHeight(32);
    pimpl_->pull_btn_->setCheckable(true);
    pimpl_->pull_btn_->setChecked(true);
    pimpl_->pull_btn_->setStyleSheet(
        "QPushButton { background: transparent; color: #a1a1aa; border: none; font-weight: bold; border-bottom: 2px solid transparent; }"
        "QPushButton:checked { color: #f97316; border-bottom: 2px solid #f97316; }"
    );
    
    pimpl_->load_btn_ = new QPushButton("Load Image");
    pimpl_->load_btn_->setCursor(Qt::PointingHandCursor);
    pimpl_->load_btn_->setFixedHeight(32);
    pimpl_->load_btn_->setCheckable(true);
    pimpl_->load_btn_->setStyleSheet(
        "QPushButton { background: transparent; color: #a1a1aa; border: none; font-weight: bold; border-bottom: 2px solid transparent; }"
        "QPushButton:checked { color: #f97316; border-bottom: 2px solid #f97316; }"
    );
    
    tabs_layout->addWidget(pimpl_->pull_btn_);
    tabs_layout->addWidget(pimpl_->load_btn_);
    tabs_layout->addStretch();
    main_layout->addLayout(tabs_layout);
    
    // Stacked Widget for forms
    pimpl_->stack_ = new QStackedWidget;
    
    // 1. Pull Form
    auto* pull_page = new QWidget;
    auto* pull_layout = new QVBoxLayout(pull_page);
    pull_layout->setContentsMargins(0, 10, 0, 0);
    pull_layout->setSpacing(14);
    
    auto* pull_row = new QHBoxLayout;
    pimpl_->pull_name_ = new QLineEdit;
    pimpl_->pull_name_->setPlaceholderText("e.g. nginx");
    pimpl_->pull_name_->setFixedHeight(36);
    
    pimpl_->pull_tag_ = new QLineEdit;
    pimpl_->pull_tag_->setPlaceholderText("latest");
    pimpl_->pull_tag_->setFixedWidth(100);
    pimpl_->pull_tag_->setFixedHeight(36);
    
    pull_row->addWidget(pimpl_->pull_name_);
    auto* colon1 = new QLabel(":"); colon1->setStyleSheet("color: #FAFAFA; font-weight: bold; font-size: 14px;"); pull_row->addWidget(colon1);
    pull_row->addWidget(pimpl_->pull_tag_);
    pull_layout->addLayout(pull_row);
    pull_layout->addStretch();
    pimpl_->stack_->addWidget(pull_page);
    
    // 2. Load Form
    auto* load_page = new QWidget;
    auto* load_layout = new QVBoxLayout(load_page);
    load_layout->setContentsMargins(0, 10, 0, 0);
    load_layout->setSpacing(14);
    
    auto* load_row1 = new QHBoxLayout;
    pimpl_->load_name_ = new QLineEdit;
    pimpl_->load_name_->setPlaceholderText("Image Name (required)");
    pimpl_->load_name_->setFixedHeight(36);
    
    pimpl_->load_tag_ = new QLineEdit;
    pimpl_->load_tag_->setPlaceholderText("Tag (optional)");
    pimpl_->load_tag_->setFixedWidth(100);
    pimpl_->load_tag_->setFixedHeight(36);
    
    load_row1->addWidget(pimpl_->load_name_);
    auto* colon2 = new QLabel(":"); colon2->setStyleSheet("color: #FAFAFA; font-weight: bold; font-size: 14px;"); load_row1->addWidget(colon2);
    load_row1->addWidget(pimpl_->load_tag_);
    load_layout->addLayout(load_row1);
    
    auto* load_row2 = new QHBoxLayout;
    pimpl_->load_path_ = new QLineEdit;
    pimpl_->load_path_->setPlaceholderText("Select Tarball Archive...");
    pimpl_->load_path_->setFixedHeight(36);
    pimpl_->load_path_->setReadOnly(true);
    
    auto* browse_btn = new QPushButton("Browse");
    browse_btn->setObjectName("SecondaryBtn");
    browse_btn->setCursor(Qt::PointingHandCursor);
    browse_btn->setFixedSize(80, 36);
    
    connect(browse_btn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, "Select Tarball", QDir::homePath(), "Tar Archives (*.tar);;All Files (*.*)");
        if (!path.isEmpty()) {
            pimpl_->load_path_->setText(path);
        }
    });
    
    load_row2->addWidget(pimpl_->load_path_);
    load_row2->addWidget(browse_btn);
    load_layout->addLayout(load_row2);
    
    load_layout->addStretch();
    pimpl_->stack_->addWidget(load_page);
    
    main_layout->addWidget(pimpl_->stack_);
    
    auto* btns = new QHBoxLayout;
    auto* cancel = new QPushButton("Cancel");
    cancel->setObjectName("SecondaryBtn");
    cancel->setCursor(Qt::PointingHandCursor);
    cancel->setFixedSize(100, 36);
    
    pimpl_->confirm_btn_ = new QPushButton("Pull");
    pimpl_->confirm_btn_->setObjectName("PrimaryButton");
    pimpl_->confirm_btn_->setCursor(Qt::PointingHandCursor);
    pimpl_->confirm_btn_->setFixedSize(100, 36);
    
    btns->addStretch();
    btns->addWidget(cancel);
    btns->addWidget(pimpl_->confirm_btn_);
    main_layout->addLayout(btns);
    
    pimpl_->overlay_ = new QWidget(this);
    pimpl_->overlay_->setFixedSize(180, 40);
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
    
    // Connections
    connect(pimpl_->pull_btn_, &QPushButton::clicked, this, [this]() {
        pimpl_->pull_btn_->setChecked(true);
        pimpl_->load_btn_->setChecked(false);
        pimpl_->confirm_btn_->setText("Pull");
        pimpl_->switch_to(0);
    });
    
    connect(pimpl_->load_btn_, &QPushButton::clicked, this, [this]() {
        pimpl_->load_btn_->setChecked(true);
        pimpl_->pull_btn_->setChecked(false);
        pimpl_->confirm_btn_->setText("Load");
        pimpl_->switch_to(1);
    });
    
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    
    connect(pimpl_->confirm_btn_, &QPushButton::clicked, this, [this]() {
        bool is_pull = (pimpl_->stack_->currentIndex() == 0);
        
        if (is_pull) {
            QString name = pimpl_->pull_name_->text().trimmed();
            if (name.isEmpty()) return;
            QString tag = pimpl_->pull_tag_->text().trimmed();
            
            // Show terminal instead of spinner overlay
            pimpl_->stack_->hide();
            
            auto* terminal = new QTextEdit(this);
            terminal->setReadOnly(true);
            terminal->setStyleSheet("QTextEdit { background: #09090b; color: #a1a1aa; border: 1px solid #3f3f46; border-radius: 6px; font-family: monospace; font-size: 11px; padding: 10px; }");
            
            auto* main_layout = static_cast<QVBoxLayout*>(layout());
            main_layout->insertWidget(2, terminal, 1);
            
            pimpl_->confirm_btn_->hide();
            
            for (auto* b : findChildren<QPushButton*>()) {
                if (b->text() == "Cancel") {
                    b->setText("Close");
                    break;
                }
            }
            
            Backend::get_instance().pull_image(name, tag);
            
            auto* conn_out = new QMetaObject::Connection();
            auto* conn_fin = new QMetaObject::Connection();
            
            *conn_out = connect(&Backend::get_instance(), &Backend::pull_output_received, this, [terminal](const QString& msg) {
                QScrollBar* vBar = terminal->verticalScrollBar();
                bool atBottom = (vBar->value() == vBar->maximum());
                
                QTextCursor cursor = terminal->textCursor();
                cursor.movePosition(QTextCursor::End);
                cursor.insertText(msg);
                
                if (atBottom) {
                    vBar->setValue(vBar->maximum());
                }
            });
            
            *conn_fin = connect(&Backend::get_instance(), &Backend::pull_finished, this, [this, terminal, conn_out, conn_fin](bool success) {
                QObject::disconnect(*conn_out);
                QObject::disconnect(*conn_fin);
                delete conn_out;
                delete conn_fin;
                
                QScrollBar* vBar = terminal->verticalScrollBar();
                bool atBottom = (vBar->value() == vBar->maximum());
                
                QTextCursor cursor = terminal->textCursor();
                cursor.movePosition(QTextCursor::End);
                
                if (success) {
                    cursor.insertText("\n--- Pull Successful ---");
                    terminal->setStyleSheet("QTextEdit { background: #09090b; color: #4ade80; border: 1px solid #3f3f46; border-radius: 6px; font-family: monospace; font-size: 11px; padding: 10px; }");
                } else {
                    cursor.insertText("\n--- Pull Failed ---");
                    terminal->setStyleSheet("QTextEdit { background: #09090b; color: #f87171; border: 1px solid #3f3f46; border-radius: 6px; font-family: monospace; font-size: 11px; padding: 10px; }");
                }
                
                if (atBottom) {
                    vBar->setValue(vBar->maximum());
                }
            });
        } else {
            QString name = pimpl_->load_name_->text().trimmed();
            QString path = pimpl_->load_path_->text().trimmed();
            if (name.isEmpty() || path.isEmpty()) return;
            QString tag = pimpl_->load_tag_->text().trimmed();
            
            pimpl_->loading_text_->setText("Loading Image...");
            pimpl_->overlay_->show();
            pimpl_->overlay_->move((width() - pimpl_->overlay_->width()) / 2, height() - pimpl_->overlay_->height() - 40);
            pimpl_->overlay_->raise();
            
            QTimer::singleShot(50, this, [this, name, tag, path]() {
                Backend::get_instance().load_image(name, tag, path);
                QTimer::singleShot(2500, this, [this]() {
                    pimpl_->overlay_->hide();
                    accept();
                });
            });
        }
    });
}

PullImageDialog::~PullImageDialog() = default;


struct BuildImageDialog::Impl {
    ToggleSwitch* output_toggle_{};
    QWidget*      image_widget_{};
    QWidget*      output_widget_{};
    QLineEdit*    name_input_{};
    QLineEdit*    tag_input_{};
    QLineEdit*    context_input_{};
    QPushButton*  context_btn_{};
    QLineEdit*    file_input_{};
    QPushButton*  file_btn_{};
    QLineEdit*    output_input_{};
    QPushButton*  output_btn_{};
    QLineEdit*    target_input_{};
    QLineEdit*    buildarg_input_{};
    ToggleSwitch* no_cache_toggle_{};
    QScrollArea*  scroll_area_{};
    QWidget*      scroll_content_{};
};

BuildImageDialog::BuildImageDialog(QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("CreateDialog");
    setWindowTitle("Build Image");
    setFixedSize(600, 750);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QLineEdit:read-only { background: #1f1f22; color: #a1a1aa; }"
    );


    auto* main { new QVBoxLayout(this) };
    main->setContentsMargins(30, 30, 30, 30);
    main->setSpacing(15);

    auto* head { new QHBoxLayout };
    head->setContentsMargins(0, 0, 0, 10);
    
    auto* title_lbl { new QLabel("Build Project Image") };
    title_lbl->setObjectName("PageTitle");
    title_lbl->setAlignment(Qt::AlignCenter);

    auto* close { new QPushButton("✕") };
    close->setObjectName("CloseBtn");
    close->setFixedSize(30, 30);
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    
    head->addStretch();
    head->addWidget(title_lbl);
    head->addStretch();
    head->addWidget(close);
    main->addLayout(head);

    auto* div1 { new QFrame };
    div1->setObjectName("Divider");
    div1->setFixedHeight(1);
    main->addWidget(div1);

    pimpl_->scroll_area_ = new QScrollArea;
    pimpl_->scroll_area_->setWidgetResizable(true);
    pimpl_->scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pimpl_->scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pimpl_->scroll_area_->setStyleSheet("QScrollArea { border: none; background: transparent; } QScrollBar:vertical { background: transparent; width: 8px; border-radius: 4px; margin: 0px; } QScrollBar::handle:vertical { background: #52525B; border-radius: 4px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");
    
    pimpl_->scroll_content_ = new QWidget;
    pimpl_->scroll_content_->setStyleSheet("background: transparent;");
    auto* v_layout { new QVBoxLayout(pimpl_->scroll_content_) };
    v_layout->setContentsMargins(0, 10, 15, 10);
    v_layout->setSpacing(20);
    pimpl_->scroll_area_->setWidget(pimpl_->scroll_content_);
    main->addWidget(pimpl_->scroll_area_);

    
    // Toggle for Output Directory
    auto* out_toggle_row { new QHBoxLayout };
    pimpl_->output_toggle_ = new ToggleSwitch;
    out_toggle_row->addWidget(pimpl_->output_toggle_);
    auto* out_toggle_lbl { new QLabel("Export to Directory Instead of Quiver image") };
    out_toggle_lbl->setObjectName("ToggleText");
    out_toggle_row->addWidget(out_toggle_lbl);
    out_toggle_row->addStretch();
    v_layout->addLayout(out_toggle_row);

    // Use a QStackedWidget to prevent layout jitter when switching
    auto* type_stack = new QStackedWidget;
    type_stack->setFixedHeight(80); // Set a fixed height to prevent any jitter

    // Image Name Widget (Default Shown)
    pimpl_->image_widget_ = new QWidget;
    auto* img_layout = new QVBoxLayout(pimpl_->image_widget_);
    img_layout->setContentsMargins(0, 0, 0, 0);
    auto* img_lbl { new QLabel("Image Name") };
    img_lbl->setObjectName("FormLabel");
    img_layout->addWidget(img_lbl);
    auto* img_row { new QHBoxLayout };
    pimpl_->name_input_ = new QLineEdit;
    pimpl_->name_input_->setPlaceholderText("abc");
    pimpl_->name_input_->setFixedHeight(36);
    pimpl_->tag_input_ = new QLineEdit;
    pimpl_->tag_input_->setObjectName("TagInput");
    pimpl_->tag_input_->setText("v1");
    pimpl_->tag_input_->setFixedSize(100, 36);
    pimpl_->tag_input_->setAlignment(Qt::AlignCenter);
    img_row->addWidget(pimpl_->name_input_);
    auto* colon { new QLabel(":") };
    colon->setObjectName("ColonLabel");
    img_row->addWidget(colon);
    img_row->addWidget(pimpl_->tag_input_);
    img_layout->addLayout(img_row);
    type_stack->addWidget(pimpl_->image_widget_);

    // Output Widget (Hidden by Default)
    pimpl_->output_widget_ = new QWidget;
    auto* out_layout = new QVBoxLayout(pimpl_->output_widget_);
    out_layout->setContentsMargins(0, 0, 0, 0);
    auto* output_lbl { new QLabel("Output Directory") };
    output_lbl->setObjectName("FormLabel");
    out_layout->addWidget(output_lbl);
    auto* out_row { new QHBoxLayout };
    pimpl_->output_input_ = new QLineEdit;
    pimpl_->output_input_->setPlaceholderText("e.g. ./temp/");
    pimpl_->output_input_->setFixedHeight(36);
    pimpl_->output_btn_ = new QPushButton("Browse...");
    pimpl_->output_btn_->setObjectName("SecondaryBtn");
    pimpl_->output_btn_->setFixedSize(100, 36);
    pimpl_->output_btn_->setCursor(Qt::PointingHandCursor);
    connect(pimpl_->output_btn_, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Directory", QDir::homePath());
        if (!dir.isEmpty()) pimpl_->output_input_->setText(dir);
    });
    out_row->addWidget(pimpl_->output_input_);
    out_row->addWidget(pimpl_->output_btn_);
    out_layout->addLayout(out_row);
    type_stack->addWidget(pimpl_->output_widget_);

    v_layout->addWidget(type_stack);

    // Toggle logic with fade animation
    connect(pimpl_->output_toggle_, &QCheckBox::toggled, this, [this, type_stack](bool checked) {
        QWidget* old_widget = checked ? pimpl_->image_widget_ : pimpl_->output_widget_;
        QWidget* new_widget = checked ? pimpl_->output_widget_ : pimpl_->image_widget_;
        
        auto* effect = new QGraphicsOpacityEffect(old_widget);
        old_widget->setGraphicsEffect(effect);
        auto* anim = new QPropertyAnimation(effect, "opacity");
        anim->setDuration(150);
        anim->setStartValue(1.0);
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, this, [this, type_stack, new_widget, old_widget]() {
            // setGraphicsEffect(nullptr) deletes the effect automatically
            old_widget->setGraphicsEffect(nullptr);
            type_stack->setCurrentWidget(new_widget);
            
            auto* effect2 = new QGraphicsOpacityEffect(new_widget);
            new_widget->setGraphicsEffect(effect2);
            auto* anim2 = new QPropertyAnimation(effect2, "opacity");
            anim2->setDuration(150);
            anim2->setStartValue(0.0);
            anim2->setEndValue(1.0);
            connect(anim2, &QPropertyAnimation::finished, this, [new_widget]() {
                new_widget->setGraphicsEffect(nullptr);
            });
            anim2->start(QAbstractAnimation::DeleteWhenStopped);
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    });

    // Context Path
    auto* context_lbl { new QLabel("Context Directory (Default)") };
    context_lbl->setObjectName("FormLabel");
    v_layout->addWidget(context_lbl);
    auto* ctx_row { new QHBoxLayout };
    pimpl_->context_input_ = new QLineEdit;
    pimpl_->context_input_->setPlaceholderText("e.g. ~/Downloads/");
    pimpl_->context_input_->setFixedHeight(36);
    pimpl_->context_btn_ = new QPushButton("Browse...");
    pimpl_->context_btn_->setObjectName("SecondaryBtn");
    pimpl_->context_btn_->setFixedSize(100, 36);
    pimpl_->context_btn_->setCursor(Qt::PointingHandCursor);
    connect(pimpl_->context_btn_, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Context Directory", QDir::homePath());
        if (!dir.isEmpty()) pimpl_->context_input_->setText(dir);
    });
    ctx_row->addWidget(pimpl_->context_input_);
    ctx_row->addWidget(pimpl_->context_btn_);
    v_layout->addLayout(ctx_row);

    // Custom File Path
    auto* file_lbl { new QLabel("Custom Quiverfile") };
    file_lbl->setObjectName("FormLabel");
    v_layout->addWidget(file_lbl);
    auto* file_row { new QHBoxLayout };
    pimpl_->file_input_ = new QLineEdit;
    pimpl_->file_input_->setPlaceholderText("Leave empty for default Quiverfile in Context Dir");
    pimpl_->file_input_->setFixedHeight(36);
    pimpl_->file_btn_ = new QPushButton("Browse...");
    pimpl_->file_btn_->setObjectName("SecondaryBtn");
    pimpl_->file_btn_->setFixedSize(100, 36);
    pimpl_->file_btn_->setCursor(Qt::PointingHandCursor);
    connect(pimpl_->file_btn_, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Select Custom File", QDir::homePath());
        if (!file.isEmpty()) pimpl_->file_input_->setText(file);
    });
    file_row->addWidget(pimpl_->file_input_);
    file_row->addWidget(pimpl_->file_btn_);
    v_layout->addLayout(file_row);

    // Target Stage & Build Args
    auto* half_row { new QHBoxLayout };
    auto* half_left { new QVBoxLayout };
    auto* target_lbl { new QLabel("Target Stage") };
    target_lbl->setObjectName("FormLabel");
    half_left->addWidget(target_lbl);
    pimpl_->target_input_ = new QLineEdit;
    pimpl_->target_input_->setPlaceholderText("e.g. build-stage");
    pimpl_->target_input_->setFixedHeight(36);
    half_left->addWidget(pimpl_->target_input_);
    half_row->addLayout(half_left);

    auto* half_right { new QVBoxLayout };
    auto* no_cache_lbl { new QLabel("Options") };
    no_cache_lbl->setObjectName("FormLabel");
    half_right->addWidget(no_cache_lbl);
    
    auto* cache_row { new QHBoxLayout };
    pimpl_->no_cache_toggle_ = new ToggleSwitch;
    cache_row->addWidget(pimpl_->no_cache_toggle_);
    auto* cache_txt { new QLabel("No Cache") };
    cache_txt->setObjectName("ToggleText");
    cache_row->addWidget(cache_txt);
    cache_row->addStretch();
    half_right->addLayout(cache_row);
    
    half_row->addLayout(half_right);
    v_layout->addLayout(half_row);

    auto* buildarg_lbl { new QLabel("Build Arguments") };
    buildarg_lbl->setObjectName("FormLabel");
    v_layout->addWidget(buildarg_lbl);
    pimpl_->buildarg_input_ = new QLineEdit;
    pimpl_->buildarg_input_->setPlaceholderText("e.g. VERSION=1.2 NAME=100");
    pimpl_->buildarg_input_->setFixedHeight(36);
    v_layout->addWidget(pimpl_->buildarg_input_);
    
    v_layout->addStretch();

    auto* foot { new QHBoxLayout };
    foot->addStretch();
    auto* create { new QPushButton("Build Image") };
    create->setObjectName("PrimaryButton");
    create->setFixedSize(160, 40);
    create->setCursor(Qt::PointingHandCursor);
    connect(create, &QPushButton::clicked, this, [this]() {
        QString ctx = pimpl_->context_input_->text().trimmed();
        QString custom_file = pimpl_->file_input_->text().trimmed();
        
        if (custom_file.isEmpty() && !ctx.isEmpty()) {
            QDir dir(ctx);
            if (!dir.exists("Quiverfile")) {
                CustomAlert alert(CustomAlert::Warning, "File Not Found", "No Quiverfile found in default location: " + ctx, this);
                alert.exec();
                return;
            }
        } else if (custom_file.isEmpty() && ctx.isEmpty()) {
            QDir dir(QDir::currentPath());
            if (!dir.exists("Quiverfile")) {
                CustomAlert alert(CustomAlert::Warning, "File Not Found", "No Quiverfile found in current directory.", this);
                alert.exec();
                return;
            }
        }
        
        QStringList args;
        args << "build";
        
        if (!pimpl_->output_toggle_->isChecked()) {
            QString name = pimpl_->name_input_->text().trimmed();
            QString tag = pimpl_->tag_input_->text().trimmed();
            if (!name.isEmpty()) {
                if (!tag.isEmpty()) {
                    args << "-t" << (name + ":" + tag);
                } else {
                    args << "-t" << name;
                }
            }
        } else {
            QString output = pimpl_->output_input_->text().trimmed();
            if (!output.isEmpty()) {
                args << "--output" << output;
            }
        }
        
        if (!custom_file.isEmpty()) {
            args << "--file" << custom_file;
        }
        
        if (pimpl_->no_cache_toggle_->isChecked()) {
            args << "--no-cache";
        }
        
        QString target = pimpl_->target_input_->text().trimmed();
        if (!target.isEmpty()) {
            args << "--target" << target;
        }
        
        QString buildargs = pimpl_->buildarg_input_->text().trimmed();
        if (!buildargs.isEmpty()) {
            QStringList parts = buildargs.split(" ", Qt::SkipEmptyParts);
            for (const QString& part : parts) {
                args << "--build-arg" << part;
            }
        }
        
        if (!ctx.isEmpty()) {
            args << ctx;
        } else {
            args << ".";
        }
        
        BuildProgressDialog progress_dialog(args, this);
        progress_dialog.exec();
        
        accept();
    });
    foot->addWidget(create);
    main->addLayout(foot);
}
BuildImageDialog::~BuildImageDialog() = default;



struct CreateDialog::Impl {
    QStackedWidget* stack_   {};
    QPushButton*  btn_visual_{};
    QPushButton*  btn_json_  {};
    QLineEdit*    name_input_{};
    QLineEdit*    image_input_{};
    QLineEdit*    tag_input_{};
    QLabel*       cpu_val_label_{};
    QLabel*       mem_val_label_{};
    QListWidget*  device_list_ {};
    QListWidget*  volume_list_ {};
    QListWidget*  port_list_   {};
    QTextEdit*    json_editor_ {};
    QComboBox*    fs_combo_    {};
    ToggleSwitch* interact_toggle_ {};
    QLineEdit*    options_input_ {};
    QLineEdit*    command_input_ {};
    QScrollArea*  scroll_area_ {};
    QWidget*      scroll_content_ {};
    QSlider*      cpu_quota_slider_{};
    QLineEdit*    cpu_quota_input_{};
    QLineEdit*    cpu_weight_input_{};
    QSlider*      memory_max_slider_{};
    QLineEdit*    memory_max_input_{};
    QLineEdit*    memory_swap_input_{};
    QLineEdit*    pids_limit_input_{};
    QLineEdit*    cpuset_cpus_input_{};
    QLineEdit*    cpuset_mems_input_{};
    QLineEdit*    io_weight_input_{};
    QLineEdit*    io_max_input_{};
};

CreateDialog::CreateDialog(QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("CreateDialog");
    setWindowTitle("Create Container");
    setFixedSize(600, 820);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QLineEdit:read-only { background: #1f1f22; color: #a1a1aa; }"
    );


    auto* main { new QVBoxLayout(this) };
    main->setContentsMargins(30, 30, 30, 30);
    main->setSpacing(15);

    auto* head { new QHBoxLayout };
    head->setContentsMargins(0, 0, 0, 10);
    auto* toggle_box { new QFrame };
    toggle_box->setObjectName("ToggleBox");
    toggle_box->setFixedSize(200, 40);
    auto* tbl { new QHBoxLayout(toggle_box) };
    tbl->setContentsMargins(4, 4, 4, 4);
    tbl->setSpacing(0);

    pimpl_->btn_visual_ = new QPushButton("VISUAL");
    pimpl_->btn_visual_->setObjectName("TabBtn");
    pimpl_->btn_visual_->setCheckable(true);
    pimpl_->btn_visual_->setChecked(true);
    pimpl_->btn_visual_->setCursor(Qt::PointingHandCursor);
    pimpl_->btn_visual_->setFixedHeight(32);
    pimpl_->btn_json_ = new QPushButton("JSON");
    pimpl_->btn_json_->setObjectName("TabBtn");
    pimpl_->btn_json_->setCheckable(true);
    pimpl_->btn_json_->setCursor(Qt::PointingHandCursor);
    pimpl_->btn_json_->setFixedHeight(32);

    auto* grp { new QButtonGroup(this) };
    grp->addButton(pimpl_->btn_visual_);
    grp->addButton(pimpl_->btn_json_);
    grp->setExclusive(true);
    connect(pimpl_->btn_visual_, &QPushButton::clicked, this, &CreateDialog::show_visual);
    connect(pimpl_->btn_json_,   &QPushButton::clicked, this, &CreateDialog::show_json);
    tbl->addWidget(pimpl_->btn_visual_);
    tbl->addWidget(pimpl_->btn_json_);

    auto* close { new QPushButton("✕") };
    close->setObjectName("CloseBtn");
    close->setFixedSize(30, 30);
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    head->addStretch();
    head->addWidget(toggle_box);
    head->addStretch();
    head->addWidget(close);
    main->addLayout(head);

    auto* div1 { new QFrame };
    div1->setObjectName("Divider");
    div1->setFixedHeight(1);
    main->addWidget(div1);

    pimpl_->stack_ = new QStackedWidget;
    main->addWidget(pimpl_->stack_);


    auto* page_visual { new QWidget };
    auto* page_visual_layout { new QVBoxLayout(page_visual) };
    page_visual_layout->setContentsMargins(0, 0, 0, 0);

    pimpl_->scroll_area_ = new QScrollArea;
    pimpl_->scroll_area_->setWidgetResizable(true);
    pimpl_->scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pimpl_->scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pimpl_->scroll_area_->setStyleSheet("QScrollArea { border: none; background: transparent; } QScrollBar:vertical { background: transparent; width: 8px; border-radius: 4px; margin: 0px; } QScrollBar::handle:vertical { background: #52525B; border-radius: 4px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");
    
    pimpl_->scroll_content_ = new QWidget;
    pimpl_->scroll_content_->setStyleSheet("background: transparent;");
    auto* v_layout { new QVBoxLayout(pimpl_->scroll_content_) };
    v_layout->setContentsMargins(0, 10, 15, 10);
    v_layout->setSpacing(20);
    pimpl_->scroll_area_->setWidget(pimpl_->scroll_content_);
    page_visual_layout->addWidget(pimpl_->scroll_area_);

    auto* core_lbl { new QLabel("CORE CONFIGURATION") };
    core_lbl->setObjectName("SectionTitle");
    v_layout->addWidget(core_lbl);

    auto* name_lbl { new QLabel("Container Name") };
    name_lbl->setObjectName("FormLabel");
    v_layout->addWidget(name_lbl);
    pimpl_->name_input_ = new QLineEdit;
    pimpl_->name_input_->setPlaceholderText("e.g. web-server-01");
    pimpl_->name_input_->setFixedHeight(36);
    v_layout->addWidget(pimpl_->name_input_);

    auto* img_lbl { new QLabel("Image") };
    img_lbl->setObjectName("FormLabel");
    v_layout->addWidget(img_lbl);
    auto* img_row { new QHBoxLayout };
    pimpl_->image_input_ = new QLineEdit;
    pimpl_->image_input_->setPlaceholderText("nginx");
    pimpl_->image_input_->setFixedHeight(36);
    pimpl_->tag_input_ = new QLineEdit;
    pimpl_->tag_input_->setObjectName("TagInput");
    pimpl_->tag_input_->setText("latest");
    pimpl_->tag_input_->setFixedSize(100, 36);
    pimpl_->tag_input_->setAlignment(Qt::AlignCenter);
    img_row->addWidget(pimpl_->image_input_);
    auto* colon { new QLabel(":") };
    colon->setObjectName("ColonLabel");
    img_row->addWidget(colon);
    img_row->addWidget(pimpl_->tag_input_);
    v_layout->addLayout(img_row);

    auto* fs_row { new QHBoxLayout };
    auto* v1 { new QVBoxLayout };
    auto* fs_lbl { new QLabel("Filesystem Type") };
    fs_lbl->setObjectName("FormLabel");
    v1->addWidget(fs_lbl);
    pimpl_->fs_combo_ = new QComboBox;
    pimpl_->fs_combo_->addItems({"OverlayFS", "Btrfs", "VFS"});
    pimpl_->fs_combo_->setFixedHeight(36);
    v1->addWidget(pimpl_->fs_combo_);
    auto* v2 { new QVBoxLayout };
    auto* persist_lbl { new QLabel("Interactions") };
    persist_lbl->setObjectName("FormLabel");
    v2->addWidget(persist_lbl);
    auto* h2 { new QHBoxLayout };
    pimpl_->interact_toggle_ = new ToggleSwitch;
    h2->addWidget(pimpl_->interact_toggle_);
    auto* pr_lbl { new QLabel("Prevent Interaction") };
    pr_lbl->setObjectName("ToggleText");
    h2->addWidget(pr_lbl);
    h2->addStretch();
    v2->addLayout(h2);
    fs_row->addLayout(v1);
    fs_row->addSpacing(20);
    fs_row->addLayout(v2);
    v_layout->addLayout(fs_row);

    auto* boxes { new QHBoxLayout };
    auto create_list_box = [&](const QString& title_text, QListWidget*& out_list, auto slot_fn) {
        auto* box { new QFrame };
        box->setObjectName("InnerBox");
        box->setFixedHeight(140);
        auto* bl { new QVBoxLayout(box) };
        bl->setContentsMargins(15, 12, 15, 12);

        
        auto* top_row { new QHBoxLayout };
        auto* b_title { new QLabel(title_text) };
        b_title->setObjectName("BoxTitle");
        top_row->addWidget(b_title);
        top_row->addStretch();

        auto* plus { new QPushButton("+") };
        plus->setObjectName("SmallPlusBtn");
        plus->setFixedSize(22, 22);
        plus->setCursor(Qt::PointingHandCursor);
        connect(plus, &QPushButton::clicked, this, slot_fn);
        top_row->addWidget(plus);
        bl->addLayout(top_row);

        
        out_list = new QListWidget;
        out_list->setObjectName("CreateList"); 
        out_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        out_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        bl->addWidget(out_list);

        boxes->addWidget(box);
    };

    create_list_box("DEVICES", pimpl_->device_list_, &CreateDialog::on_add_device);
    create_list_box("MOUNTS", pimpl_->volume_list_, &CreateDialog::on_add_volume);
    create_list_box("PORTS",   pimpl_->port_list_,   &CreateDialog::on_add_port);
    v_layout->addLayout(boxes);

    auto* res_lbl { new QLabel("RESOURCE LIMITATIONS") };
    res_lbl->setObjectName("SectionTitle");
    v_layout->addWidget(res_lbl);

    auto* rc1 { new QHBoxLayout };

    auto* v_cwgt { new QVBoxLayout };
    auto* cweight_lbl { new QLabel("CPU Weight") };
    cweight_lbl->setObjectName("FormLabel");
    pimpl_->cpu_weight_input_ = new QLineEdit;
    pimpl_->cpu_weight_input_->setPlaceholderText("e.g. 1 - 10000");
    pimpl_->cpu_weight_input_->setFixedHeight(36);
    connect(pimpl_->cpu_weight_input_, &QLineEdit::textChanged, pimpl_->cpu_weight_input_, [this](const QString& text){
        if (text.isEmpty()) return;
        bool ok; int val = text.toInt(&ok);
        if (ok && val > 10000) pimpl_->cpu_weight_input_->setText("10000");
    });
    v_cwgt->addWidget(cweight_lbl);
    v_cwgt->addWidget(pimpl_->cpu_weight_input_);
    rc1->addLayout(v_cwgt);
    v_layout->addLayout(rc1);

    auto* cquota_lbl { new QLabel("CPU Quota (%)") };
    cquota_lbl->setObjectName("FormLabel");
    v_layout->addWidget(cquota_lbl);
    auto* rc_q { new QHBoxLayout };
    pimpl_->cpu_quota_slider_ = new QSlider(Qt::Horizontal);
    pimpl_->cpu_quota_slider_->setRange(0, 100);
    pimpl_->cpu_quota_input_ = new QLineEdit;
    pimpl_->cpu_quota_input_->setPlaceholderText("e.g. 1 - 100%");
    pimpl_->cpu_quota_input_->setFixedHeight(36);
    pimpl_->cpu_quota_input_->setFixedWidth(120);
    connect(pimpl_->cpu_quota_slider_, &QSlider::valueChanged, pimpl_->cpu_quota_input_, [this](int val){
        if (val == 0) pimpl_->cpu_quota_input_->clear();
        else pimpl_->cpu_quota_input_->setText(QString::number(val));
    });
    connect(pimpl_->cpu_quota_input_, &QLineEdit::textChanged, pimpl_->cpu_quota_slider_, [this](const QString& text){
        int val = text.toInt();
        if (val > 100) { val = 100; pimpl_->cpu_quota_input_->setText("100"); }
        pimpl_->cpu_quota_slider_->setValue(val);
    });
    rc_q->addWidget(pimpl_->cpu_quota_slider_);
    rc_q->addWidget(pimpl_->cpu_quota_input_);
    v_layout->addLayout(rc_q);

    auto* mmax_lbl { new QLabel("Memory Max (MB)") };
    mmax_lbl->setObjectName("FormLabel");
    v_layout->addWidget(mmax_lbl);
    auto* rc3 { new QHBoxLayout };
    pimpl_->memory_max_slider_ = new QSlider(Qt::Horizontal);
    pimpl_->memory_max_slider_->setRange(0, 2147483647);
    pimpl_->memory_max_input_ = new QLineEdit;
    pimpl_->memory_max_input_->setPlaceholderText("e.g. 1 - 17592186044415 MB");
    pimpl_->memory_max_input_->setFixedHeight(36);
    pimpl_->memory_max_input_->setFixedWidth(200);
    connect(pimpl_->memory_max_slider_, &QSlider::valueChanged, pimpl_->memory_max_input_, [this](int val){
        if (val == 0) pimpl_->memory_max_input_->clear();
        else pimpl_->memory_max_input_->setText(QString::number(val));
    });
    connect(pimpl_->memory_max_input_, &QLineEdit::textChanged, pimpl_->memory_max_slider_, [this](const QString& text){
        quint64 val = text.toULongLong();
        if (val > 17592186044415ULL) { val = 17592186044415ULL; pimpl_->memory_max_input_->setText("17592186044415"); }
        pimpl_->memory_max_slider_->setValue(val > 2147483647 ? 2147483647 : static_cast<int>(val));
    });
    rc3->addWidget(pimpl_->memory_max_slider_);
    rc3->addWidget(pimpl_->memory_max_input_);
    v_layout->addLayout(rc3);

    auto* rc_s { new QHBoxLayout };
    auto* v_mswap { new QVBoxLayout };
    auto* mswap_lbl { new QLabel("Memory Swap") };
    mswap_lbl->setObjectName("FormLabel");
    pimpl_->memory_swap_input_ = new QLineEdit;
    pimpl_->memory_swap_input_->setPlaceholderText("e.g. 1g");
    pimpl_->memory_swap_input_->setFixedHeight(36);
    v_mswap->addWidget(mswap_lbl);
    v_mswap->addWidget(pimpl_->memory_swap_input_);
    rc_s->addLayout(v_mswap);

    auto* v_pids { new QVBoxLayout };
    auto* pids_lbl { new QLabel("PIDs Limit") };
    pids_lbl->setObjectName("FormLabel");
    pimpl_->pids_limit_input_ = new QLineEdit;
    pimpl_->pids_limit_input_->setPlaceholderText("e.g. 1 - 4194304");
    pimpl_->pids_limit_input_->setFixedHeight(36);
    connect(pimpl_->pids_limit_input_, &QLineEdit::textChanged, pimpl_->pids_limit_input_, [this](const QString& text){
        if (text.isEmpty()) return;
        bool ok; int val = text.toInt(&ok);
        if (ok && val > 4194304) pimpl_->pids_limit_input_->setText("4194304");
    });
    v_pids->addWidget(pids_lbl);
    v_pids->addWidget(pimpl_->pids_limit_input_);
    rc_s->addLayout(v_pids);
    v_layout->addLayout(rc_s);

    auto* rc5 { new QHBoxLayout };
    auto* cpus_lbl { new QLabel("Cpuset CPUs") };
    cpus_lbl->setObjectName("FormLabel");
    pimpl_->cpuset_cpus_input_ = new QLineEdit;
    pimpl_->cpuset_cpus_input_->setPlaceholderText("e.g. 0,1 or 0-3");
    pimpl_->cpuset_cpus_input_->setFixedHeight(36);
    rc5->addWidget(cpus_lbl);
    rc5->addWidget(pimpl_->cpuset_cpus_input_);
    
    auto* mems_lbl { new QLabel("Cpuset Mems") };
    mems_lbl->setObjectName("FormLabel");
    pimpl_->cpuset_mems_input_ = new QLineEdit;
    pimpl_->cpuset_mems_input_->setPlaceholderText("e.g. 0,1 or 0-3");
    pimpl_->cpuset_mems_input_->setFixedHeight(36);
    rc5->addWidget(mems_lbl);
    rc5->addWidget(pimpl_->cpuset_mems_input_);
    v_layout->addLayout(rc5);

    auto* rc6 { new QHBoxLayout };
    auto* iow_lbl { new QLabel("IO Weight") };
    iow_lbl->setObjectName("FormLabel");
    pimpl_->io_weight_input_ = new QLineEdit;
    pimpl_->io_weight_input_->setPlaceholderText("MAJOR:MINOR:WEIGHT");
    pimpl_->io_weight_input_->setFixedHeight(36);
    rc6->addWidget(iow_lbl);
    rc6->addWidget(pimpl_->io_weight_input_);
    
    auto* iom_lbl { new QLabel("IO Max") };
    iom_lbl->setObjectName("FormLabel");
    pimpl_->io_max_input_ = new QLineEdit;
    pimpl_->io_max_input_->setPlaceholderText("MAJOR:MINOR:RBPS...");
    pimpl_->io_max_input_->setFixedHeight(36);
    rc6->addWidget(iom_lbl);
    rc6->addWidget(pimpl_->io_max_input_);
    v_layout->addLayout(rc6);


    auto* opt_lbl { new QLabel("Options") };
    opt_lbl->setObjectName("FormLabel");
    v_layout->addWidget(opt_lbl);
    pimpl_->options_input_ = new QLineEdit;
    pimpl_->options_input_->setPlaceholderText("e.g. --privileged, --env KEY=value, etc.");
    pimpl_->options_input_->setFixedHeight(36);
    v_layout->addWidget(pimpl_->options_input_);

    auto* cmd_lbl { new QLabel("Commands") };
    cmd_lbl->setObjectName("FormLabel");
    v_layout->addWidget(cmd_lbl);
    pimpl_->command_input_ = new QLineEdit;
    pimpl_->command_input_->setPlaceholderText("e.g. bash, python script.py, etc.");
    pimpl_->command_input_->setFixedHeight(36);
    v_layout->addWidget(pimpl_->command_input_);

    pimpl_->stack_->addWidget(page_visual);


    auto* page_json { new QWidget };
    auto* j_layout { new QVBoxLayout(page_json) };
    j_layout->setContentsMargins(0, 10, 0, 0);
    auto* j_head { new QHBoxLayout };
    auto* f_name { new QLabel("config.json") };
    f_name->setObjectName("FileNameLabel");
    auto* imp_btn { new QPushButton("Import JSON File") };
    imp_btn->setObjectName("SecondaryBtn");
    imp_btn->setCursor(Qt::PointingHandCursor);
    imp_btn->setFixedSize(160, 36);
    connect(imp_btn, &QPushButton::clicked, this, &CreateDialog::on_import_json);
    j_head->addWidget(f_name);
    j_head->addStretch();
    j_head->addWidget(imp_btn);
    j_layout->addLayout(j_head);

   
    pimpl_->json_editor_ = new QTextEdit;
    pimpl_->json_editor_->setObjectName("JsonEditor");
    
    // Using a C++ raw string literal (R"(...)") makes formatting JSON much cleaner
    pimpl_->json_editor_->setText(R"({
    "container_id": "",
    "hostname": "quiver-node",
    "domain_name": "",
    "vfs": false,
    "rootfs": {
        "path": "/var/lib/quiver/rootfs",
        "readonly": false
    },
    "terminal": true,
    "detach": false,
    "console_size": {
        "height": 0,
        "width": 0
    },
    "user": {
        "uid": 0,
        "gid": 0
    },
    "uid_mapping": [],
    "gid_mapping": [],
    "env": [
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
    ],
    "cwd": "/",
    "args": [
        "/bin/sh"
    ],
    "oom_score": 0,
    "no_new_privileges": true,
    "capabilities": [],
    "rlimits": [],
    "cgroups_path": "",
    "devices": [],
    "networks": {},
    "timeoffsets": [],
    "namespaces": [
        { "type": "pid" },
        { "type": "network" },
        { "type": "ipc" },
        { "type": "uts" },
        { "type": "mount" }
    ],
    "mounts": [],
    "masked_paths": [],
    "read_only_paths": []
})");
    j_layout->addWidget(pimpl_->json_editor_);
    pimpl_->stack_->addWidget(page_json);


    auto* foot { new QHBoxLayout };
    foot->addStretch();
    auto* create { new QPushButton("Create Container") };
    create->setObjectName("PrimaryButton");
    create->setFixedSize(160, 40);
    create->setCursor(Qt::PointingHandCursor);
    connect(create, &QPushButton::clicked, this, [this]() {
        auto validate_int = [&](QLineEdit* field, quint64 max_val, const QString& name, quint64 min_val = 0) -> bool {
            QString txt = field->text().trimmed();
            if (txt.isEmpty()) return true;
            bool ok;
            quint64 val = txt.toULongLong(&ok);
            if (!ok || val < min_val || val > max_val) {
                QMessageBox::warning(this, "Validation Error", 
                    QString("Invalid value for %1. Must be between %2 and %3.").arg(name).arg(min_val).arg(max_val));
                return false;
            }
            return true;
        };
        
        if (!validate_int(pimpl_->cpu_quota_input_, 100, "CPU Quota (%)", 1)) return;
        if (!validate_int(pimpl_->cpu_weight_input_, 10000, "CPU Weight", 1)) return;
        if (!validate_int(pimpl_->memory_max_input_, 17592186044415ULL, "Memory Max", 1)) return;
        if (!validate_int(pimpl_->pids_limit_input_, 4194304, "PIDs Limit", 0)) return;
        if (!validate_int(pimpl_->io_weight_input_, 1000, "IO Weight", 1)) return;
        if (!validate_int(pimpl_->io_max_input_, 17592186044415ULL, "IO Max", 1)) return;
        
        accept();
    });
    foot->addWidget(create);
    main->addLayout(foot);
}
CreateDialog::~CreateDialog() = default;

auto CreateDialog::get_container_name()  const -> QString { return pimpl_->name_input_->text(); }
auto CreateDialog::get_container_image() const -> QString { 
    QString img = pimpl_->image_input_->text().trimmed();
    QString tag = pimpl_->tag_input_->text().trimmed();
    if (tag.isEmpty()) tag = "latest";
    if (img.isEmpty()) return "";
    return img + ":" + tag;
}

auto CreateDialog::get_devices() const -> QStringList {
    QStringList list;
    for (int i = 0; i < pimpl_->device_list_->count(); ++i) {
        list << pimpl_->device_list_->item(i)->text();
    }
    return list;
}

auto CreateDialog::get_volumes() const -> QStringList {
    QStringList list;
    for (int i = 0; i < pimpl_->volume_list_->count(); ++i) {
        list << pimpl_->volume_list_->item(i)->text();
    }
    return list;
}

auto CreateDialog::get_ports() const -> QStringList {
    QStringList list;
    for (int i = 0; i < pimpl_->port_list_->count(); ++i) {
        list << pimpl_->port_list_->item(i)->text();
    }
    return list;
}

auto CreateDialog::get_filesystem() const -> QString {
    return pimpl_->fs_combo_->currentText();
}

auto CreateDialog::get_prevent_interaction() const -> bool {
    return pimpl_->interact_toggle_->isChecked();
}

auto CreateDialog::get_command() const -> QString {
    return pimpl_->command_input_->text().trimmed();
}

auto CreateDialog::get_options() const -> QString {
    return pimpl_->options_input_->text().trimmed();
}

auto CreateDialog::get_cpu_quota() const -> int { 
    QString txt = pimpl_->cpu_quota_input_->text().trimmed();
    if (txt.isEmpty()) return 0;
    return txt.toInt() * 1000; 
}
auto CreateDialog::get_cpu_weight() const -> int { return pimpl_->cpu_weight_input_->text().toInt(); }
auto CreateDialog::get_memory_max() const -> QString {
    quint64 val = pimpl_->memory_max_input_->text().toULongLong();
    if (val > 0) return QString::number(val * 1048576ULL); // Convert MB to Bytes
    return "";
}
auto CreateDialog::get_memory_swap() const -> QString { return pimpl_->memory_swap_input_->text().trimmed(); }
auto CreateDialog::get_pids_limit() const -> int { return pimpl_->pids_limit_input_->text().toInt(); }
auto CreateDialog::get_cpuset_cpus() const -> QString { return pimpl_->cpuset_cpus_input_->text().trimmed(); }
auto CreateDialog::get_cpuset_mems() const -> QString { return pimpl_->cpuset_mems_input_->text().trimmed(); }
auto CreateDialog::get_io_weight() const -> QString { return pimpl_->io_weight_input_->text().trimmed(); }
auto CreateDialog::get_io_max() const -> QString {
    quint64 val = pimpl_->io_max_input_->text().toULongLong();
    if (val > 0) return QString::number(val * 1048576ULL);
    return "";
}

auto CreateDialog::show_visual() -> void { pimpl_->stack_->setCurrentIndex(0); }
auto CreateDialog::show_json()   -> void { pimpl_->stack_->setCurrentIndex(1); }

auto CreateDialog::on_add_device() -> void {
    QDialog d(this);
    d.setWindowTitle("Select Devices");
    d.setFixedSize(320, 380);
    d.setObjectName("SubDialog");
    auto* l { new QVBoxLayout(&d) };
    l->addWidget(new QLabel("Select multiple devices:"));
    auto* list { new QListWidget(&d) };
    list->setSelectionMode(QAbstractItemView::MultiSelection);
    list->addItems({"/dev/ttyUSB0 (Serial)", "/dev/video0 (Camera)",
                    "/dev/dri/card0 (GPU)",  "/dev/snd (Audio)",
                    "/dev/sda1 (Drive)"});
    l->addWidget(list);
    // auto* box { new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d) };
    // connect(box, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    // connect(box, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    // l->addWidget(box);

    auto* btns { new QHBoxLayout };
    auto* cancel_btn { new QPushButton("Cancel") };
    cancel_btn->setObjectName("SecondaryBtn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedSize(85, 34); // Nice and wide!
    connect(cancel_btn, &QPushButton::clicked, &d, &QDialog::reject);

    auto* ok_btn { new QPushButton("OK") };
    ok_btn->setObjectName("PrimaryButton");
    ok_btn->setCursor(Qt::PointingHandCursor);
    ok_btn->setFixedSize(85, 34); // Nice and wide!
    connect(ok_btn, &QPushButton::clicked, &d, &QDialog::accept);

    btns->addStretch();
    btns->addWidget(cancel_btn);
    btns->addWidget(ok_btn);
    l->addLayout(btns);

    if (d.exec() == QDialog::Accepted) {
        for (QListWidgetItem* item : list->selectedItems()) {
            add_item_with_delete_btn(pimpl_->device_list_, item->text());
        }
    }
}

auto CreateDialog::on_add_volume() -> void {
    QDialog d(this);
    d.setWindowTitle("Add Volume Mapping");
    d.setFixedSize(400, 300);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    d.setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QPushButton#SecondaryBtn { background: transparent; color: #fafafa; border: 1px solid #3f3f46; border-radius: 6px; font-weight: bold; }"
        "QPushButton#SecondaryBtn:hover { background: #27272a; }"
        "QPushButton#PrimaryButton { background: #ea580c; color: #fafafa; border: none; border-radius: 6px; font-weight: bold; }"
        "QPushButton#PrimaryButton:hover { background: #f97316; }"
    );
    d.setAttribute(Qt::WA_TranslucentBackground);
    
    auto* base_l { new QVBoxLayout(&d) };
    base_l->setContentsMargins(10, 10, 10, 10);
    auto* bg_frame { new QFrame(&d) };
    bg_frame->setObjectName("PopupFrame");
    base_l->addWidget(bg_frame);
    auto* l { new QVBoxLayout(bg_frame) };
    l->setContentsMargins(20, 20, 20, 20);
    l->setSpacing(15);
    
    auto* title_lbl { new QLabel("Add Volume") };
    title_lbl->setStyleSheet("color: #fafafa; font-size: 16px; font-weight: bold;");
    l->addWidget(title_lbl);
    
    auto* host_lbl { new QLabel("Host Path:") };
    host_lbl->setStyleSheet("color: #a1a1aa; font-size: 13px;");
    l->addWidget(host_lbl);
    
    auto* host_row { new QHBoxLayout };
    auto* host_input { new QLineEdit };
    host_input->setPlaceholderText("e.g. /home/user/data");
    host_input->setFixedHeight(36);
    auto* browse_btn { new QPushButton("...") };
    browse_btn->setFixedSize(36, 36);
    browse_btn->setObjectName("SecondaryBtn");
    browse_btn->setCursor(Qt::PointingHandCursor);
    QObject::connect(browse_btn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&d, "Select Host Mount Directory", "", QFileDialog::ShowDirsOnly);
        if (!dir.isEmpty()) host_input->setText(dir);
    });
    host_row->addWidget(host_input);
    host_row->addWidget(browse_btn);
    l->addLayout(host_row);
    
    auto* cont_lbl { new QLabel("Container Path:") };
    cont_lbl->setStyleSheet("color: #a1a1aa; font-size: 13px;");
    l->addWidget(cont_lbl);
    
    auto* cont_input { new QLineEdit };
    cont_input->setPlaceholderText("e.g. /app/data");
    cont_input->setFixedHeight(36);
    l->addWidget(cont_input);
    
    l->addStretch();
    auto* btns { new QHBoxLayout };
    auto* cancel_btn { new QPushButton("Cancel") };
    cancel_btn->setObjectName("SecondaryBtn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedSize(85, 34);
    QObject::connect(cancel_btn, &QPushButton::clicked, &d, &QDialog::reject);
    auto* add_btn { new QPushButton("Add") };
    add_btn->setObjectName("PrimaryButton");
    add_btn->setCursor(Qt::PointingHandCursor);
    add_btn->setFixedSize(85, 34);
    QObject::connect(add_btn, &QPushButton::clicked, &d, &QDialog::accept);
    btns->addStretch();
    btns->addWidget(cancel_btn);
    btns->addWidget(add_btn);
    l->addLayout(btns);
    
    if (d.exec() == QDialog::Accepted) {
        QString host = host_input->text().trimmed();
        QString cont = cont_input->text().trimmed();
        if (!host.isEmpty() && !cont.isEmpty()) {
            add_item_with_delete_btn(pimpl_->volume_list_, host + ":" + cont);
        }
    }
}

auto CreateDialog::on_add_port() -> void {
    QDialog d(this);
    d.setWindowTitle("Add Port Mapping");
    d.setFixedSize(360, 240);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    d.setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QLineEdit:read-only { background: #1f1f22; color: #a1a1aa; }"
    );

    d.setAttribute(Qt::WA_TranslucentBackground);
    auto* base_l { new QVBoxLayout(&d) };
    base_l->setContentsMargins(10, 10, 10, 10);
    auto* bg_frame { new QFrame(&d) };
    
    bg_frame->setObjectName("PopupFrame");
    base_l->addWidget(bg_frame);
    auto* l { new QVBoxLayout(bg_frame) };
    l->setContentsMargins(20, 20, 20, 20);
    l->setSpacing(15);
    auto* title_lbl { new QLabel("Add Port") };
    title_lbl->setObjectName("PageTitle");
    l->addWidget(title_lbl);
    auto* label { new QLabel("Enter mapping (HostPort:ContainerPort)") };
    label->setStyleSheet("color: #a1a1aa; font-size: 13px;");
    l->addWidget(label);
    auto* port_input { new QLineEdit(bg_frame) };
    port_input->setPlaceholderText("e.g. 8080:80");
    port_input->setFixedHeight(36);
    l->addWidget(port_input);
    l->addStretch();
    auto* btns { new QHBoxLayout };
    auto* cancel_btn { new QPushButton("Cancel") };
    cancel_btn->setObjectName("SecondaryBtn");
    cancel_btn->setCursor(Qt::PointingHandCursor);
    cancel_btn->setFixedSize(85, 34);
    connect(cancel_btn, &QPushButton::clicked, &d, &QDialog::reject);
    auto* add_btn { new QPushButton("Add Port") };
    add_btn->setObjectName("PrimaryButton");
    add_btn->setCursor(Qt::PointingHandCursor);
    add_btn->setFixedSize(85, 34);
    connect(add_btn, &QPushButton::clicked, &d, &QDialog::accept);
    btns->addStretch();
    btns->addWidget(cancel_btn);
    btns->addWidget(add_btn);
    l->addLayout(btns);
    if (d.exec() == QDialog::Accepted) {
        QString txt { port_input->text().trimmed() };
        if (!txt.isEmpty()) {
            add_item_with_delete_btn(pimpl_->port_list_, txt);
        }
    }
}

auto CreateDialog::on_import_json() -> void {
    QString file_name { QFileDialog::getOpenFileName(
        this, "Import JSON Config", "", "JSON Files (*.json);;All Files (*)") };
    if (!file_name.isEmpty()) {
        QFile file(file_name);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            pimpl_->json_editor_->setText(in.readAll());
            file.close();
        }
    }
}

struct CustomAlert::Impl {
    QLabel* title_lbl_{};
    QLabel* msg_lbl_{};
};

CustomAlert::CustomAlert(Type type, const QString& title, const QString& message, QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QLineEdit:read-only { background: #1f1f22; color: #a1a1aa; }"
    );

    setFixedSize(400, 200);
    setObjectName("CreateDialog");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(15);

    pimpl_->title_lbl_ = new QLabel(title);
    pimpl_->title_lbl_->setObjectName("PageTitle");
    layout->addWidget(pimpl_->title_lbl_);

    pimpl_->msg_lbl_ = new QLabel(message);
    pimpl_->msg_lbl_->setStyleSheet("color: #a1a1aa; font-size: 14px;");
    pimpl_->msg_lbl_->setWordWrap(true);
    layout->addWidget(pimpl_->msg_lbl_);
    layout->addStretch();

    auto* btns = new QHBoxLayout;
    btns->addStretch();

    if (type == Question) {
        auto* cancel = new QPushButton("No");
        cancel->setObjectName("SecondaryBtn");
        cancel->setCursor(Qt::PointingHandCursor);
        cancel->setFixedSize(85, 34);
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

        auto* yes_btn = new QPushButton("Yes");
        yes_btn->setObjectName("PrimaryButton");
        yes_btn->setCursor(Qt::PointingHandCursor);
        yes_btn->setFixedSize(85, 34);
        connect(yes_btn, &QPushButton::clicked, this, &QDialog::accept);

        btns->addWidget(cancel);
        btns->addWidget(yes_btn);
    } else if (type == Warning) {
        auto* ok_btn = new QPushButton("OK");
        ok_btn->setObjectName("PrimaryButton");
        ok_btn->setCursor(Qt::PointingHandCursor);
        ok_btn->setFixedSize(85, 34);
        connect(ok_btn, &QPushButton::clicked, this, &QDialog::accept);

        btns->addWidget(ok_btn);
    }
    
    layout->addLayout(btns);
}
CustomAlert::~CustomAlert() = default;

struct CircularGauge::Impl {
    QString title_;
    double soft_limit_;
    double hard_limit_;
};

CircularGauge::CircularGauge(const QString& title, double soft_limit, double hard_limit, QWidget* parent)
    : QWidget(parent), pimpl_{std::make_unique<Impl>()} 
{
    pimpl_->title_ = title;
    pimpl_->soft_limit_ = soft_limit;
    pimpl_->hard_limit_ = hard_limit;
    setFixedSize(160, 160);
}

CircularGauge::~CircularGauge() = default;

void CircularGauge::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int size = qMin(width(), height());
    int thickness = 12;
    QRectF rect(thickness, thickness, size - thickness * 2, size - thickness * 2);

    // Draw background circle
    QPen bg_pen(QColor("#27272a"), thickness);
    bg_pen.setCapStyle(Qt::RoundCap);
    painter.setPen(bg_pen);
    painter.drawArc(rect, 0, 360 * 16);

    // Calculate angles
    double ratio = (pimpl_->hard_limit_ > 0) ? (pimpl_->soft_limit_ / pimpl_->hard_limit_) : 0.0;
    if (ratio > 1.0) ratio = 1.0;
    int span_angle = -static_cast<int>(ratio * 360 * 16);

    // Draw progress arc with solid color
    QPen fg_pen(QColor("#f97316"), thickness);
    fg_pen.setCapStyle(Qt::RoundCap);
    painter.setPen(fg_pen);
    painter.drawArc(rect, 90 * 16, span_angle); // Start at top (90 deg)

    // Draw text
    painter.setPen(QColor("#ffffff"));
    QFont font = painter.font();
    
    // Adjust rect so text doesn't overlap the arc
    QRectF text_rect = rect.adjusted(10, 25, -10, -25);

    font.setPixelSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(text_rect, Qt::AlignHCenter | Qt::AlignTop, pimpl_->title_);

    font.setPixelSize(16);
    font.setBold(false);
    painter.setFont(font);
    
    // For large numbers like 524288, use a smaller font or break it up if needed.
    // We can just print the soft limit for the main value, or "soft / hard"
    QString text = QString::number(pimpl_->soft_limit_) + "\n/\n" + QString::number(pimpl_->hard_limit_);
    painter.drawText(text_rect, Qt::AlignCenter, text);
}

struct UpdateDialog::Impl {
    QSlider*      cpu_quota_slider_{};
    QLineEdit*    cpu_quota_input_{};
    QLineEdit*    cpu_weight_input_{};
    QSlider*      memory_max_slider_{};
    QLineEdit*    memory_max_input_{};
    QLineEdit*    memory_swap_input_{};
    QLineEdit*    pids_limit_input_{};
    QLineEdit*    cpuset_cpus_input_{};
    QLineEdit*    cpuset_mems_input_{};
    QLineEdit*    io_weight_input_{};
    QLineEdit*    io_max_input_{};
    Quiver::Container container_;
};

UpdateDialog::UpdateDialog(const Quiver::Container& c, QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    pimpl_->container_ = c;
    setObjectName("CreateDialog");
    setWindowTitle("Update Container: " + c.name);
    setFixedSize(600, 750);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    setStyleSheet(
        "QDialog { background: #18181b; border: 1px solid #27272a; border-radius: 8px; }"
        "QLineEdit { background: #09090b; border: 1px solid #27272a; border-radius: 6px; color: #fafafa; padding: 0 10px; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #f97316; }"
        "QLineEdit:read-only { background: #1f1f22; color: #a1a1aa; }"
    );


    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(30, 30, 30, 30);
    main->setSpacing(15);

    auto* head { new QHBoxLayout };
    head->setContentsMargins(0, 0, 0, 10);
    
    auto* title = new QLabel(c.name);
    title->setStyleSheet("color: #a1a1aa; font-size: 14px; font-weight: bold;");
    head->addWidget(title);
    head->addStretch();
    
    auto* close { new QPushButton("✕") };
    close->setObjectName("CloseBtn");
    close->setFixedSize(30, 30);
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    head->addWidget(close);
    main->addLayout(head);

    auto* div1 { new QFrame };
    div1->setFrameShape(QFrame::HLine);
    div1->setObjectName("Divider");
    main->addWidget(div1);

    auto* scroll_area = new QScrollArea;
    scroll_area->setWidgetResizable(true);
    scroll_area->setObjectName("DialogScroll");
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setStyleSheet("QScrollArea { border: none; background: transparent; } QScrollBar:vertical { background: transparent; width: 8px; border-radius: 4px; margin: 0px; } QScrollBar::handle:vertical { background: #52525B; border-radius: 4px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }");
    
    auto* scroll_content = new QWidget;
    scroll_content->setObjectName("DialogScrollContent");
    scroll_content->setStyleSheet("background: transparent;");
    auto* v_layout = new QVBoxLayout(scroll_content);
    v_layout->setContentsMargins(0, 10, 15, 10);
    v_layout->setSpacing(20);
    scroll_area->setWidget(scroll_content);
    main->addWidget(scroll_area);

    auto* res_lbl { new QLabel("RESOURCE LIMITATIONS") };
    res_lbl->setObjectName("SectionTitle");
    v_layout->addWidget(res_lbl);

    auto* rc1 { new QHBoxLayout };
    auto* v_cwgt { new QVBoxLayout };
    auto* cweight_lbl { new QLabel("CPU Weight") };
    cweight_lbl->setObjectName("FormLabel");
    pimpl_->cpu_weight_input_ = new QLineEdit;
    pimpl_->cpu_weight_input_->setPlaceholderText("e.g. 1 - 10000");
    pimpl_->cpu_weight_input_->setFixedHeight(36);
    if (c.cpu_weight > 0) pimpl_->cpu_weight_input_->setText(QString::number(c.cpu_weight));
    connect(pimpl_->cpu_weight_input_, &QLineEdit::textChanged, pimpl_->cpu_weight_input_, [this](const QString& text){
        if (text.isEmpty()) return;
        bool ok; int val = text.toInt(&ok);
        if (ok && val > 10000) pimpl_->cpu_weight_input_->setText("10000");
    });
    v_cwgt->addWidget(cweight_lbl);
    v_cwgt->addWidget(pimpl_->cpu_weight_input_);
    rc1->addLayout(v_cwgt);
    v_layout->addLayout(rc1);

    auto* cquota_lbl { new QLabel("CPU Quota (%)") };
    cquota_lbl->setObjectName("FormLabel");
    v_layout->addWidget(cquota_lbl);
    auto* rc_q { new QHBoxLayout };
    pimpl_->cpu_quota_slider_ = new QSlider(Qt::Horizontal);
    pimpl_->cpu_quota_slider_->setRange(0, 100);
    pimpl_->cpu_quota_input_ = new QLineEdit;
    pimpl_->cpu_quota_input_->setPlaceholderText("e.g. 1 - 100%");
    pimpl_->cpu_quota_input_->setFixedHeight(36);
    pimpl_->cpu_quota_input_->setFixedWidth(120);
    if (c.cpu_quota > 0) pimpl_->cpu_quota_input_->setText(QString::number(c.cpu_quota / 1000));
    connect(pimpl_->cpu_quota_slider_, &QSlider::valueChanged, pimpl_->cpu_quota_input_, [this](int val){
        if (val == 0) pimpl_->cpu_quota_input_->clear();
        else pimpl_->cpu_quota_input_->setText(QString::number(val));
    });
    connect(pimpl_->cpu_quota_input_, &QLineEdit::textChanged, pimpl_->cpu_quota_slider_, [this](const QString& text){
        int val = text.toInt();
        if (val > 100) { val = 100; pimpl_->cpu_quota_input_->setText("100"); }
        pimpl_->cpu_quota_slider_->setValue(val);
    });
    rc_q->addWidget(pimpl_->cpu_quota_slider_);
    rc_q->addWidget(pimpl_->cpu_quota_input_);
    v_layout->addLayout(rc_q);

    auto* mmax_lbl { new QLabel("Memory Max (MB)") };
    mmax_lbl->setObjectName("FormLabel");
    v_layout->addWidget(mmax_lbl);
    auto* rc3 { new QHBoxLayout };
    pimpl_->memory_max_slider_ = new QSlider(Qt::Horizontal);
    pimpl_->memory_max_slider_->setRange(0, 2147483647);
    pimpl_->memory_max_input_ = new QLineEdit;
    pimpl_->memory_max_input_->setPlaceholderText("e.g. 1 - 17592186044415 MB");
    pimpl_->memory_max_input_->setFixedHeight(36);
    pimpl_->memory_max_input_->setFixedWidth(200);
    if (!c.memory_max.isEmpty()) {
        quint64 bytes = c.memory_max.toULongLong();
        if (bytes > 0) pimpl_->memory_max_input_->setText(QString::number(bytes / 1048576ULL));
    }
    connect(pimpl_->memory_max_slider_, &QSlider::valueChanged, pimpl_->memory_max_input_, [this](int val){
        if (val == 0) pimpl_->memory_max_input_->clear();
        else pimpl_->memory_max_input_->setText(QString::number(val));
    });
    connect(pimpl_->memory_max_input_, &QLineEdit::textChanged, pimpl_->memory_max_slider_, [this](const QString& text){
        quint64 val = text.toULongLong();
        if (val > 17592186044415ULL) { val = 17592186044415ULL; pimpl_->memory_max_input_->setText("17592186044415"); }
        pimpl_->memory_max_slider_->setValue(val > 2147483647 ? 2147483647 : static_cast<int>(val));
    });
    rc3->addWidget(pimpl_->memory_max_slider_);
    rc3->addWidget(pimpl_->memory_max_input_);
    v_layout->addLayout(rc3);

    auto* rc_s { new QHBoxLayout };
    auto* v_mswap { new QVBoxLayout };
    auto* mswap_lbl { new QLabel("Memory Swap") };
    mswap_lbl->setObjectName("FormLabel");
    pimpl_->memory_swap_input_ = new QLineEdit;
    pimpl_->memory_swap_input_->setPlaceholderText("e.g. 1g");
    pimpl_->memory_swap_input_->setFixedHeight(36);
    pimpl_->memory_swap_input_->setText(c.memory_swap);
    v_mswap->addWidget(mswap_lbl);
    v_mswap->addWidget(pimpl_->memory_swap_input_);
    rc_s->addLayout(v_mswap);

    auto* v_pids { new QVBoxLayout };
    auto* pids_lbl { new QLabel("PIDs Limit") };
    pids_lbl->setObjectName("FormLabel");
    pimpl_->pids_limit_input_ = new QLineEdit;
    pimpl_->pids_limit_input_->setPlaceholderText("e.g. 0 - 4194304");
    pimpl_->pids_limit_input_->setFixedHeight(36);
    if (c.pids_limit > 0) pimpl_->pids_limit_input_->setText(QString::number(c.pids_limit));
    connect(pimpl_->pids_limit_input_, &QLineEdit::textChanged, pimpl_->pids_limit_input_, [this](const QString& text){
        if (text.isEmpty()) return;
        bool ok; quint64 val = text.toULongLong(&ok);
        if (ok && val > 4194304) pimpl_->pids_limit_input_->setText("4194304");
    });
    v_pids->addWidget(pids_lbl);
    v_pids->addWidget(pimpl_->pids_limit_input_);
    rc_s->addLayout(v_pids);
    v_layout->addLayout(rc_s);

    auto* rc5 { new QHBoxLayout };
    auto* cpus_lbl { new QLabel("Cpuset CPUs") };
    cpus_lbl->setObjectName("FormLabel");
    pimpl_->cpuset_cpus_input_ = new QLineEdit;
    pimpl_->cpuset_cpus_input_->setPlaceholderText("e.g. 0,1 or 0-3");
    pimpl_->cpuset_cpus_input_->setFixedHeight(36);
    pimpl_->cpuset_cpus_input_->setText(c.cpuset_cpus);
    rc5->addWidget(cpus_lbl);
    rc5->addWidget(pimpl_->cpuset_cpus_input_);
    
    auto* mems_lbl { new QLabel("Cpuset Mems") };
    mems_lbl->setObjectName("FormLabel");
    pimpl_->cpuset_mems_input_ = new QLineEdit;
    pimpl_->cpuset_mems_input_->setPlaceholderText("e.g. 0,1 or 0-3");
    pimpl_->cpuset_mems_input_->setFixedHeight(36);
    pimpl_->cpuset_mems_input_->setText(c.cpuset_mems);
    rc5->addWidget(mems_lbl);
    rc5->addWidget(pimpl_->cpuset_mems_input_);
    v_layout->addLayout(rc5);

    auto* rc6 { new QHBoxLayout };
    auto* iow_lbl { new QLabel("IO Weight") };
    iow_lbl->setObjectName("FormLabel");
    pimpl_->io_weight_input_ = new QLineEdit;
    pimpl_->io_weight_input_->setPlaceholderText("1 - 1000");
    pimpl_->io_weight_input_->setFixedHeight(36);
    pimpl_->io_weight_input_->setText(c.io_weight);
    rc6->addWidget(iow_lbl);
    rc6->addWidget(pimpl_->io_weight_input_);
    
    auto* iom_lbl { new QLabel("IO Max") };
    iom_lbl->setObjectName("FormLabel");
    pimpl_->io_max_input_ = new QLineEdit;
    pimpl_->io_max_input_->setPlaceholderText("1 - 17592186044415");
    pimpl_->io_max_input_->setFixedHeight(36);
    if (!c.io_max.isEmpty()) {
        quint64 bytes = c.io_max.toULongLong();
        if (bytes > 0) pimpl_->io_max_input_->setText(QString::number(bytes / 1048576ULL));
    }
    rc6->addWidget(iom_lbl);
    rc6->addWidget(pimpl_->io_max_input_);
    v_layout->addLayout(rc6);

    v_layout->addStretch();

    auto* foot { new QHBoxLayout };
    foot->addStretch();
    auto* create { new QPushButton("Update Container") };
    create->setObjectName("PrimaryButton");
    create->setFixedSize(160, 40);
    create->setCursor(Qt::PointingHandCursor);
    connect(create, &QPushButton::clicked, this, [this]() {
        auto validate_int = [&](QLineEdit* field, quint64 max_val, const QString& name, quint64 min_val = 0) -> bool {
            QString txt = field->text().trimmed();
            if (txt.isEmpty()) return true;
            bool ok;
            quint64 val = txt.toULongLong(&ok);
            if (!ok || val < min_val || val > max_val) {
                QMessageBox::warning(this, "Validation Error", 
                    QString("Invalid value for %1. Must be between %2 and %3.").arg(name).arg(min_val).arg(max_val));
                return false;
            }
            return true;
        };
        
        if (!validate_int(pimpl_->cpu_quota_input_, 100, "CPU Quota (%)", 1)) return;
        if (!validate_int(pimpl_->cpu_weight_input_, 10000, "CPU Weight", 1)) return;
        if (!validate_int(pimpl_->memory_max_input_, 17592186044415ULL, "Memory Max", 1)) return;
        if (!validate_int(pimpl_->pids_limit_input_, 4194304, "PIDs Limit", 0)) return;
        if (!validate_int(pimpl_->io_weight_input_, 1000, "IO Weight", 1)) return;
        if (!validate_int(pimpl_->io_max_input_, 17592186044415ULL, "IO Max", 1)) return;
        
        accept();
    });
    foot->addWidget(create);
    main->addLayout(foot);
}
UpdateDialog::~UpdateDialog() = default;

auto UpdateDialog::get_cpu_quota() const -> int { 
    QString txt = pimpl_->cpu_quota_input_->text().trimmed();
    if (txt.isEmpty()) return 0;
    return txt.toInt() * 1000; 
}
auto UpdateDialog::get_cpu_weight() const -> int { return pimpl_->cpu_weight_input_->text().toInt(); }
auto UpdateDialog::get_memory_max() const -> QString {
    quint64 val = pimpl_->memory_max_input_->text().toULongLong();
    if (val > 0) return QString::number(val * 1048576ULL); // Convert MB to Bytes
    return "";
}
auto UpdateDialog::get_memory_swap() const -> QString { return pimpl_->memory_swap_input_->text().trimmed(); }
auto UpdateDialog::get_pids_limit() const -> int { return pimpl_->pids_limit_input_->text().toInt(); }
auto UpdateDialog::get_cpuset_cpus() const -> QString { return pimpl_->cpuset_cpus_input_->text().trimmed(); }
auto UpdateDialog::get_cpuset_mems() const -> QString { return pimpl_->cpuset_mems_input_->text().trimmed(); }
auto UpdateDialog::get_io_weight() const -> QString { return pimpl_->io_weight_input_->text().trimmed(); }
auto UpdateDialog::get_io_max() const -> QString {
    quint64 val = pimpl_->io_max_input_->text().toULongLong();
    if (val > 0) return QString::number(val * 1048576ULL);
    return "";
}

void CreateDialog::set_config(const QJsonObject& config) {
    if (config.contains("image")) {
        QString full_image = config["image"].toString();
        int colon_idx = full_image.indexOf(':');
        if (colon_idx != -1) {
            pimpl_->image_input_->setText(full_image.left(colon_idx));
            pimpl_->tag_input_->setText(full_image.mid(colon_idx + 1));
        } else {
            pimpl_->image_input_->setText(full_image);
            pimpl_->tag_input_->setText("latest");
        }
    }
    if (config.contains("cpu_quota")) {
        pimpl_->cpu_quota_input_->setText(config["cpu_quota"].toString());
        pimpl_->cpu_quota_slider_->setValue(config["cpu_quota"].toString().toInt());
    }
    if (config.contains("memory_limit")) {
        // Assume bytes in config, convert back to MB for slider/input
        quint64 val = config["memory_limit"].toString().toULongLong();
        if (val > 0) {
            quint64 mb = val / 1048576ULL;
            pimpl_->memory_max_input_->setText(QString::number(mb));
            pimpl_->memory_max_slider_->setValue(mb);
        }
    }
    if (config.contains("ports")) {
        QJsonArray ports = config["ports"].toArray();
        for (int i = 0; i < ports.size(); ++i) {
            add_item_with_delete_btn(pimpl_->port_list_, ports[i].toString());
        }
    }
}

} // namespace Quiver
