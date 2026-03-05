#include "include/TablePages.h"
#include "include/Components.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QFrame>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QWidget>
#include <QAbstractItemView>
#include <QSizePolicy>

namespace Quiver {
namespace {

auto make_item(const QString& text, bool bright = false) -> QTableWidgetItem* {
    auto* item { new QTableWidgetItem(text) };
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setForeground(bright ? QColor{"#e2e8f0"} : QColor{"#94a3b8"});
    return item;
}

auto make_status_badge(const QString& status) -> QWidget* {
    bool positive {
        status.toLower() == "active"   ||
        status.toLower() == "open"     ||
        status.toLower() == "mounted"  ||
        status.toLower() == "running"  ||
        status.toLower() == "in use"
    };

    QString bg  { positive ? "rgba(74,222,128,0.12)"  : "rgba(251,113,133,0.12)" };
    QString fg  { positive ? "#4ade80"                : "#fb7185"                };

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
};

TablePage::TablePage(const QString& title,
                     const QStringList& columns,
                     QWidget* parent)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{
    auto* layout { new QVBoxLayout(this) };
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);


    auto* title_lbl { new QLabel(title) };
    title_lbl->setObjectName("PageTitle");
    title_lbl->setContentsMargins(0, 0, 0, 16);
    layout->addWidget(title_lbl);


    QStringList all_cols { columns };
    all_cols << "ACTIONS";

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
    pimpl_->table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pimpl_->table_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    pimpl_->table_->horizontalHeader()->setHighlightSections(false);
    pimpl_->table_->horizontalHeader()->setMinimumSectionSize(80);


    pimpl_->table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    int action_col { static_cast<int>(all_cols.size()) - 1 };
    pimpl_->table_->horizontalHeader()->setSectionResizeMode(
        action_col, QHeaderView::Fixed);

    pimpl_->table_->setColumnWidth(action_col, 130);

    layout->addWidget(pimpl_->table_, 1);


    auto* fab_row { new QHBoxLayout };
    fab_row->setContentsMargins(0, 16, 0, 0);
    pimpl_->add_btn_ = new QPushButton("+");
    pimpl_->add_btn_->setObjectName("FabButton");
    pimpl_->add_btn_->setCursor(Qt::PointingHandCursor);
    pimpl_->add_btn_->setFixedSize(52, 52);
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
                break;
            }
        }
    });
}

auto TablePage::table() -> QTableWidget* { return pimpl_->table_; }


static auto make_stat_row(
    const QString& t1, const QString& v1,
    const QString& t2, const QString& v2, const QString& c2,
    const QString& t3, const QString& v3, const QString& c3,
    QVBoxLayout* root) -> void
{
    auto* row { new QHBoxLayout };
    row->setSpacing(20);
    row->setContentsMargins(0, 0, 0, 24);

    auto make_card = [](const QString& title, const QString& value,
                        const QString& val_color) -> QFrame*
    {
        auto* card { new QFrame };
        card->setObjectName("StatPanel");
        card->setFixedHeight(90);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* l { new QVBoxLayout(card) };
        l->setAlignment(Qt::AlignCenter);
        l->setSpacing(6);

        auto* t { new QLabel(title) };
        t->setObjectName("StatLabelTitle");
        t->setAlignment(Qt::AlignCenter);

        auto* v { new QLabel(value) };
        v->setObjectName("StatLabelValue");
        v->setAlignment(Qt::AlignCenter);
        if (val_color != "#ffffff")
            v->setStyleSheet(QString("color: %1;").arg(val_color));

        l->addWidget(t);
        l->addWidget(v);
        return card;
    };

    row->addWidget(make_card(t1, v1, "#ffffff"));
    row->addWidget(make_card(t2, v2, c2));
    row->addWidget(make_card(t3, v3, c3));
    root->addLayout(row);
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
        { "REPOSITORY", "STATUS", "TAG", "IMAGE ID", "SIZE", "CREATED" },
        this);

    auto add = [&](const QStringList& d) {
        pimpl_->page_->add_row(d, "Delete", "TableDangerBtn");
    };
    add({ "nginx",    "active",   "alpine",    "a6bd71f48f68", "23.5 MB", "2 days ago"  });
    add({ "redis",    "active",   "6.2",       "7614ae9453d1", "113 MB",  "1 week ago"  });
    add({ "postgres", "active",   "14",        "d3b0b5c6a2f3", "376 MB",  "3 days ago"  });
    add({ "ubuntu",   "inactive", "22.04",     "8f70a8b0e4c1", "77.8 MB", "5 days ago"  });
    add({ "node",     "active",   "18-alpine", "9c2d4e6f0a1b", "168 MB",  "1 day ago"   });
    add({ "python",   "inactive", "3.11-slim", "b5e7f9a3c2d4", "125 MB",  "4 days ago"  });

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
        auto* cancel { new QPushButton("Cancel") }; cancel->setObjectName("SecondaryBtn"); cancel->setCursor(Qt::PointingHandCursor);
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        auto* pull { new QPushButton("Pull") }; pull->setObjectName("PrimaryButton"); pull->setCursor(Qt::PointingHandCursor);
        connect(pull, &QPushButton::clicked, &d, &QDialog::accept);
        btns->addStretch(); btns->addWidget(cancel); btns->addWidget(pull);
        l->addLayout(btns);
        if (d.exec() == QDialog::Accepted) {
            QString name { name_in->text().trimmed() };
            if (name.isEmpty()) return;
            QString tag { tag_in->text().trimmed() };
            if (tag.isEmpty()) tag = "latest";
            pimpl_->page_->add_row({ name, "active", tag, "pending...", "—", "just now" },
                                   "Delete", "TableDangerBtn");
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
        { "NAME", "STATUS", "DRIVER", "MOUNT POINT", "SIZE", "CREATED" },
        this);

    auto add = [&](const QStringList& d) {
        pimpl_->page_->add_row(d, "Unmount", "TableDangerBtn");
    };
    add({ "postgres_data", "mounted",  "local", "/var/lib/docker/volumes/postgres_data", "2.3 GB",  "3 days ago" });
    add({ "redis_cache",   "mounted",  "local", "/var/lib/docker/volumes/redis_cache",   "128 MB",  "1 week ago" });
    add({ "nginx_conf",    "inactive", "local", "/var/lib/docker/volumes/nginx_conf",    "4.2 MB",  "5 days ago" });
    add({ "app_uploads",   "mounted",  "nfs",   "/mnt/nfs/uploads",                      "14.7 GB", "2 days ago" });

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
        auto* cancel { new QPushButton("Cancel") }; cancel->setObjectName("SecondaryBtn"); cancel->setCursor(Qt::PointingHandCursor);
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        auto* create { new QPushButton("Create") }; create->setObjectName("PrimaryButton"); create->setCursor(Qt::PointingHandCursor);
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


struct PortsPage::Impl { TablePage* page_ {}; };

PortsPage::PortsPage(QWidget* parent)
    : QWidget(parent), pimpl_{ std::make_unique<Impl>() }
{
    auto* root { new QVBoxLayout(this) };
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    make_stat_row(
        "TOTAL PORTS", "5",
        "OPEN",        "4", "#4ade80",
        "CLOSED",      "1", "#fb7185",
        root);

    pimpl_->page_ = new TablePage(
        "Ports",
        { "CONTAINER", "STATUS", "HOST PORT", "CONTAINER PORT", "PROTOCOL", "BOUND TO" },
        this);

    auto add = [&](const QStringList& d) {
        pimpl_->page_->add_row(d, "Delete", "TableDangerBtn");
    };
    add({ "nginx-proxy", "open",   "80",   "80",   "TCP", "0.0.0.0"   });
    add({ "nginx-proxy", "open",   "443",  "443",  "TCP", "0.0.0.0"   });
    add({ "postgres-db", "open",   "5432", "5432", "TCP", "127.0.0.1" });
    add({ "redis-cache", "closed", "6379", "6379", "TCP", "127.0.0.1" });
    add({ "app-server",  "open",   "8080", "3000", "TCP", "0.0.0.0"   });

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
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        auto* add_btn { new QPushButton("Add") }; add_btn->setObjectName("PrimaryButton"); add_btn->setCursor(Qt::PointingHandCursor);
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
}
PortsPage::~PortsPage() = default;


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
        auto* cancel { new QPushButton("Cancel") }; cancel->setObjectName("SecondaryBtn"); cancel->setCursor(Qt::PointingHandCursor);
        connect(cancel, &QPushButton::clicked, &d, &QDialog::reject);
        auto* add_btn { new QPushButton("Add") }; add_btn->setObjectName("PrimaryButton"); add_btn->setCursor(Qt::PointingHandCursor);
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
    auto* layout { new QVBoxLayout(this) };
    layout->setContentsMargins(40, 32, 40, 32);
    layout->setSpacing(24);
    auto* title { new QLabel("Settings") };
    title->setObjectName("PageTitle");
    layout->addWidget(title);

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
