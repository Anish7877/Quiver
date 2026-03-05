#include "include/Components.h"
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
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QRandomGenerator>

namespace Quiver {

struct ToggleSwitch::Impl {};
ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QCheckBox(parent), pimpl_{std::make_unique<Impl>()}
{
    setCursor(Qt::PointingHandCursor);
    setFixedSize(50, 26);
}
ToggleSwitch::~ToggleSwitch() = default;

auto ToggleSwitch::paintEvent(QPaintEvent*) -> void {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    bool on { isChecked() };
    QColor bg { on ? QColor{"#8b5cf6"} : QColor{"#52525b"} };
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

struct ContainerCard::Impl {
    Container data_ {};
    QPushButton* menu_btn_ {};
    QPushButton* icon_box_ {};
    QLabel* status_dot_ {};
    QLabel* status_text_ {};
};

ContainerCard::ContainerCard(const Container& container_data, QWidget* parent)
    : QFrame(parent), pimpl_{std::make_unique<Impl>()}
{
    pimpl_->data_ = container_data;
    setObjectName("ContentCard");
    setFixedSize(300, 110);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout { new QVBoxLayout(this) };
    layout->setContentsMargins(15, 15, 15, 15);

    auto* top { new QHBoxLayout };
    pimpl_->status_dot_  = new QLabel;
    pimpl_->status_dot_->setFixedSize(8, 8);
    pimpl_->status_text_ = new QLabel;

    pimpl_->menu_btn_ = new QPushButton("⋮");
    pimpl_->menu_btn_->setObjectName("CardMenu");
    pimpl_->menu_btn_->setFixedSize(24, 24);
    pimpl_->menu_btn_->setCursor(Qt::PointingHandCursor);
    connect(pimpl_->menu_btn_, &QPushButton::clicked, this, &ContainerCard::show_menu);

    top->addWidget(pimpl_->status_dot_);
    top->addWidget(pimpl_->status_text_);
    top->addStretch();
    top->addWidget(pimpl_->menu_btn_);

    auto* mid { new QHBoxLayout };
    pimpl_->icon_box_ = new QPushButton;
    pimpl_->icon_box_->setFixedSize(28, 28);
    pimpl_->icon_box_->setCursor(Qt::PointingHandCursor);
    connect(pimpl_->icon_box_, &QPushButton::clicked, this, &ContainerCard::toggle_status);

    auto* name { new QLabel(pimpl_->data_.name) };
    name->setObjectName("CardName");
    mid->addWidget(pimpl_->icon_box_);
    mid->addWidget(name);
    mid->addStretch();

    auto* bot { new QHBoxLayout };
    auto* img { new QLabel(pimpl_->data_.image) };
    img->setStyleSheet("color: #71717a; font-family: monospace; font-size: 11px;");
    auto* id_hash { new QLabel(pimpl_->data_.id) };
    id_hash->setStyleSheet("color: #71717a; font-family: monospace; font-size: 11px;");
    bot->addWidget(img);
    bot->addStretch();
    bot->addWidget(id_hash);

    layout->addLayout(top);
    layout->addLayout(mid);
    layout->addStretch();
    layout->addLayout(bot);

    bool is_running { pimpl_->data_.status == "running" };
    QString color { is_running ? "#4ade80" : "#fb7185" };
    pimpl_->status_dot_->setStyleSheet(
        QString("background: %1; border-radius: 4px;").arg(color));
    pimpl_->status_text_->setText(pimpl_->data_.status.toUpper());
    pimpl_->status_text_->setStyleSheet(
        QString("color: %1; font-weight: bold; font-size: 10px; margin-left: 5px;").arg(color));

    pimpl_->icon_box_->setText(is_running ? "■" : "▶");
    QString icon_style { is_running
        ? "QPushButton { border: 1px solid #ef4444; color: #ef4444; border-radius: 14px; background: transparent; padding-bottom: 2px; } QPushButton:hover { background: #ef4444; color: white; }"
        : "QPushButton { border: 1px solid #22c55e; color: #22c55e; border-radius: 14px; background: transparent; padding-left: 2px; } QPushButton:hover { background: #22c55e; color: white; }" };
    pimpl_->icon_box_->setStyleSheet(icon_style);
}
ContainerCard::~ContainerCard() = default;

auto ContainerCard::toggle_status() -> void {
    bool will_run { pimpl_->data_.status != "running" };
    pimpl_->data_.status = will_run ? "running" : "stopped";
    QString color { will_run ? "#4ade80" : "#fb7185" };
    pimpl_->status_dot_->setStyleSheet(
        QString("background: %1; border-radius: 4px;").arg(color));
    pimpl_->status_text_->setText(pimpl_->data_.status.toUpper());
    pimpl_->status_text_->setStyleSheet(
        QString("color: %1; font-weight: bold; font-size: 10px; margin-left: 5px;").arg(color));
    pimpl_->icon_box_->setText(will_run ? "■" : "▶");
    QString icon_style { will_run
        ? "QPushButton { border: 1px solid #ef4444; color: #ef4444; border-radius: 14px; background: transparent; padding-bottom: 2px; } QPushButton:hover { background: #ef4444; color: white; }"
        : "QPushButton { border: 1px solid #22c55e; color: #22c55e; border-radius: 14px; background: transparent; padding-left: 2px; } QPushButton:hover { background: #22c55e; color: white; }" };
    pimpl_->icon_box_->setStyleSheet(icon_style);
}

auto ContainerCard::show_menu() -> void {
    QMenu menu(this);
    QAction* start_action { menu.addAction("Start") };
    QAction* stop_action  { menu.addAction("Stop") };
    menu.addSeparator();
    QAction* del_action { menu.addAction("Delete Container") };
    QAction* selected { menu.exec(QCursor::pos()) };
    if (selected == del_action) {
        on_delete();
    } else if (selected == start_action && pimpl_->data_.status != "running") {
        toggle_status();
    } else if (selected == stop_action && pimpl_->data_.status == "running") {
        toggle_status();
    }
}

auto ContainerCard::on_delete() -> void {
    DeleteDialog dialog(pimpl_->data_.name, this);
    if (dialog.exec() == QDialog::Accepted) {
        Backend::get_instance().delete_container(pimpl_->data_.id);
        emit state_changed();
    }
}


struct StatCard::Impl {};
StatCard::StatCard(const QString& title, const QString& value,
                   const QString& color, QWidget* parent)
    : QFrame(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("StatPanel");
    setMinimumHeight(80);
    setMinimumWidth(140);
    auto* layout { new QVBoxLayout(this) };
    layout->setAlignment(Qt::AlignCenter);
    auto* t { new QLabel(title) };
    t->setObjectName("StatLabelTitle");
    auto* v { new QLabel(value) };
    v->setObjectName("StatLabelValue");
    if (color != "#ffffff") v->setStyleSheet(QString("color: %1;").arg(color));
    layout->addWidget(t);
    layout->addWidget(v);
    layout->setAlignment(t, Qt::AlignCenter);
    layout->setAlignment(v, Qt::AlignCenter);
}
StatCard::~StatCard() = default;


struct ResourceTable::Impl {};

ResourceTable::ResourceTable(const QStringList& headers, QWidget* parent)
    : QTableWidget(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("ResourceTable");
    setColumnCount(static_cast<int>(headers.size()));
    setHorizontalHeaderLabels(headers);


    setShowGrid(false);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFocusPolicy(Qt::NoFocus);
    verticalHeader()->setVisible(false);


    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);


    verticalHeader()->setDefaultSectionSize(44);
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


struct CreateDialog::Impl {
    QStackedWidget* stack_   {};
    QPushButton*  btn_visual_{};
    QPushButton*  btn_json_  {};
    QLineEdit*    name_input_{};
    QLineEdit*    image_input_{};
    QLabel*       cpu_val_label_{};
    QLabel*       mem_val_label_{};
    QListWidget*  device_list_ {};
    QListWidget*  volume_list_ {};
    QListWidget*  port_list_   {};
    QTextEdit*    json_editor_ {};
};

CreateDialog::CreateDialog(QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    setObjectName("CreateDialog");
    setWindowTitle("Create Container");
    setFixedSize(600, 720);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

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
    pimpl_->btn_json_ = new QPushButton("JSON");
    pimpl_->btn_json_->setObjectName("TabBtn");
    pimpl_->btn_json_->setCheckable(true);
    pimpl_->btn_json_->setCursor(Qt::PointingHandCursor);

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
    auto* v_layout { new QVBoxLayout(page_visual) };
    v_layout->setContentsMargins(0, 10, 0, 0);
    v_layout->setSpacing(15);

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
    auto* tag { new QLineEdit };
    tag->setObjectName("TagInput");
    tag->setText("latest");
    tag->setFixedSize(100, 36);
    tag->setAlignment(Qt::AlignCenter);
    img_row->addWidget(pimpl_->image_input_);
    auto* colon { new QLabel(":") };
    colon->setObjectName("ColonLabel");
    img_row->addWidget(colon);
    img_row->addWidget(tag);
    v_layout->addLayout(img_row);

    auto* fs_row { new QHBoxLayout };
    auto* v1 { new QVBoxLayout };
    auto* fs_lbl { new QLabel("Filesystem Type") };
    fs_lbl->setObjectName("FormLabel");
    v1->addWidget(fs_lbl);
    auto* cb { new QComboBox };
    cb->addItems({"OverlayFS", "Btrfs", "VFS"});
    cb->setFixedHeight(36);
    v1->addWidget(cb);
    auto* v2 { new QVBoxLayout };
    auto* persist_lbl { new QLabel("Persistence") };
    persist_lbl->setObjectName("FormLabel");
    v2->addWidget(persist_lbl);
    auto* h2 { new QHBoxLayout };
    h2->addWidget(new ToggleSwitch);
    auto* pr_lbl { new QLabel("Prevent Removal") };
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
        box->setFixedHeight(130);
        auto* bl { new QVBoxLayout(box) };
        bl->setContentsMargins(15, 15, 15, 15);
        auto* b_title { new QLabel(title_text) };
        b_title->setObjectName("BoxTitle");
        bl->addWidget(b_title);
        out_list = new QListWidget;
        out_list->setStyleSheet(
            "QListWidget { background: transparent; border: none; outline: none; }"
            "QListWidget::item { font-family: monospace; font-size: 12px; padding: 2px 0; }"
            "QListWidget::item:selected { color: white; background: transparent; }");
        out_list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        out_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        bl->addWidget(out_list);
        auto* plus { new QPushButton("+") };
        plus->setObjectName("PlusBtn");
        plus->setFixedSize(28, 28);
        plus->setCursor(Qt::PointingHandCursor);
        connect(plus, &QPushButton::clicked, this, slot_fn);
        bl->addWidget(plus, 0, Qt::AlignRight);
        boxes->addWidget(box);
    };

    create_list_box("DEVICES", pimpl_->device_list_, &CreateDialog::on_add_device);
    create_list_box("VOLUMES", pimpl_->volume_list_, &CreateDialog::on_add_volume);
    create_list_box("PORTS",   pimpl_->port_list_,   &CreateDialog::on_add_port);
    v_layout->addLayout(boxes);

    auto* res_lbl { new QLabel("RESOURCE LIMITS") };
    res_lbl->setObjectName("SectionTitle");
    v_layout->addWidget(res_lbl);

    auto add_slider = [&](const QString& label_text, int min, int max, int val, QLabel*& lbl) {
        auto* r { new QHBoxLayout };
        auto* s_label { new QLabel(label_text) };
        s_label->setFixedWidth(90);
        s_label->setObjectName("FormLabel");
        r->addWidget(s_label);
        auto* s { new QSlider(Qt::Horizontal) };
        s->setRange(min, max);
        s->setValue(val);
        s->setCursor(Qt::PointingHandCursor);
        r->addWidget(s);
        lbl = new QLabel(QString::number(val));
        lbl->setObjectName("SliderVal");
        lbl->setFixedSize(45, 26);
        lbl->setAlignment(Qt::AlignCenter);
        r->addWidget(lbl);
        v_layout->addLayout(r);
        return s;
    };

    QSlider* s_cpu { add_slider("CPUs", 1, 24, 1, pimpl_->cpu_val_label_) };
    connect(s_cpu, &QSlider::valueChanged, this, &CreateDialog::update_cpu_label);
    update_cpu_label(1);
    QSlider* s_mem { add_slider("Memory (MB)", 128, 8192, 512, pimpl_->mem_val_label_) };
    connect(s_mem, &QSlider::valueChanged, this, &CreateDialog::update_mem_label);
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
    connect(imp_btn, &QPushButton::clicked, this, &CreateDialog::on_import_json);
    j_head->addWidget(f_name);
    j_head->addStretch();
    j_head->addWidget(imp_btn);
    j_layout->addLayout(j_head);

    pimpl_->json_editor_ = new QTextEdit;
    pimpl_->json_editor_->setObjectName("JsonEditor");
    pimpl_->json_editor_->setText(
        "{\n    \"name\": \"\",\n    \"image\": \"\",\n"
        "    \"tag\": \"latest\",\n    \"cpu_limit\": 1,\n    \"mem_limit\": 512\n}");
    j_layout->addWidget(pimpl_->json_editor_);
    pimpl_->stack_->addWidget(page_json);


    auto* foot { new QHBoxLayout };
    foot->addStretch();
    auto* create { new QPushButton("Create Container") };
    create->setObjectName("PrimaryButton");
    create->setFixedSize(160, 40);
    create->setCursor(Qt::PointingHandCursor);
    connect(create, &QPushButton::clicked, this, &QDialog::accept);
    foot->addWidget(create);
    main->addLayout(foot);
}
CreateDialog::~CreateDialog() = default;

auto CreateDialog::get_container_name()  const -> QString { return pimpl_->name_input_->text(); }
auto CreateDialog::get_container_image() const -> QString { return pimpl_->image_input_->text(); }
auto CreateDialog::update_cpu_label(int val) -> void { pimpl_->cpu_val_label_->setText(QString::number(val)); }
auto CreateDialog::update_mem_label(int val) -> void { pimpl_->mem_val_label_->setText(QString::number(val)); }
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
    auto* box { new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &d) };
    connect(box, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    l->addWidget(box);
    if (d.exec() == QDialog::Accepted) {
        for (QListWidgetItem* item : list->selectedItems()) {
            if (pimpl_->device_list_->findItems(item->text(), Qt::MatchExactly).isEmpty())
                pimpl_->device_list_->addItem(item->text());
        }
    }
}

auto CreateDialog::on_add_volume() -> void {
    QString dir { QFileDialog::getExistingDirectory(
        this, "Select Volume Directory", "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks) };
    if (!dir.isEmpty() &&
        pimpl_->volume_list_->findItems(dir, Qt::MatchExactly).isEmpty())
        pimpl_->volume_list_->addItem(dir);
}

auto CreateDialog::on_add_port() -> void {
    QDialog d(this);
    d.setWindowTitle("Add Port Mapping");
    d.setFixedSize(360, 240);
    d.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    d.setAttribute(Qt::WA_TranslucentBackground);
    auto* base_l { new QVBoxLayout(&d) };
    base_l->setContentsMargins(10, 10, 10, 10);
    auto* bg_frame { new QFrame(&d) };
    bg_frame->setObjectName("CreateDialog");
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
    connect(cancel_btn, &QPushButton::clicked, &d, &QDialog::reject);
    auto* add_btn { new QPushButton("Add Port") };
    add_btn->setObjectName("PrimaryButton");
    add_btn->setCursor(Qt::PointingHandCursor);
    connect(add_btn, &QPushButton::clicked, &d, &QDialog::accept);
    btns->addStretch();
    btns->addWidget(cancel_btn);
    btns->addWidget(add_btn);
    l->addLayout(btns);
    if (d.exec() == QDialog::Accepted) {
        QString txt { port_input->text().trimmed() };
        if (!txt.isEmpty() &&
            pimpl_->port_list_->findItems(txt, Qt::MatchExactly).isEmpty())
            pimpl_->port_list_->addItem(txt);
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

}
