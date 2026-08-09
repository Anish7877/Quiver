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
#include <QGraphicsDropShadowEffect>
#include <QPainterPath>
#include <QSettings>
#include <QApplication>
#include <QClipboard>
#include <QPointer>
#include <QTimer>

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
    create_list_box("VOLUMES", pimpl_->volume_list_, &CreateDialog::on_add_volume);
    create_list_box("PORTS",   pimpl_->port_list_,   &CreateDialog::on_add_port);
    v_layout->addLayout(boxes);


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
    connect(create, &QPushButton::clicked, this, &QDialog::accept);
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
