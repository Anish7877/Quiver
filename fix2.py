import re

with open('src/TablePages.cpp', 'r') as f:
    text = f.read()

# Update TablePage::set_action_column_width
find_str = '''void TablePage::set_action_column_width(int width) {
    if (table()->columnCount() > 0) {
        table()->horizontalHeader()->setSectionResizeMode(table()->columnCount() - 1, QHeaderView::Fixed);
        table()->setColumnWidth(table()->columnCount() - 1, width);
    }
}'''
replace_str = '''void TablePage::set_action_column_width(int /*width*/) {
    if (table()->columnCount() > 0) {
        table()->horizontalHeader()->setSectionResizeMode(table()->columnCount() - 1, QHeaderView::ResizeToContents);
    }
}'''
text = text.replace(find_str, replace_str)

# Now change the signal connections in the four page constructors
# They currently look like:
#     connect(&Backend::get_instance(), &Backend::cli_action_success, this, [this](const QString&) {
#         pimpl_->overlay_->hide();
#         refresh();
#     });
#     connect(&Backend::get_instance(), &Backend::cli_error_occurred, this, [this](const QString&) {
#         pimpl_->overlay_->hide();
#         refresh();
#     });
pattern = re.compile(r'connect\(&Backend::get_instance\(\), &Backend::cli_action_success, this, \[this\]\(const QString&\) \{\s*pimpl_->overlay_->hide\(\);\s*refresh\(\);\s*\}\);\s*connect\(&Backend::get_instance\(\), &Backend::cli_error_occurred, this, \[this\]\(const QString&\) \{\s*pimpl_->overlay_->hide\(\);\s*refresh\(\);\s*\}\);')

new_connections = '''connect(&Backend::get_instance(), &Backend::cli_action_success, this, [this](const QString& msg) {
        if (pimpl_->overlay_->isVisible()) {
            pimpl_->loading_text_->setText("Success");
            pimpl_->loading_text_->setStyleSheet("color: #4ade80; font-size: 14px; font-weight: bold;");
            QTimer::singleShot(1500, this, [this]() {
                pimpl_->overlay_->hide();
                pimpl_->loading_text_->setText("Processing...");
                pimpl_->loading_text_->setStyleSheet("color: #e4e4e7; font-size: 14px; font-weight: bold;");
                refresh();
            });
        }
    });
    connect(&Backend::get_instance(), &Backend::cli_error_occurred, this, [this](const QString& err) {
        if (pimpl_->overlay_->isVisible()) {
            pimpl_->loading_text_->setText("Error");
            pimpl_->loading_text_->setStyleSheet("color: #fb7185; font-size: 14px; font-weight: bold;");
            QTimer::singleShot(2000, this, [this]() {
                pimpl_->overlay_->hide();
                pimpl_->loading_text_->setText("Processing...");
                pimpl_->loading_text_->setStyleSheet("color: #e4e4e7; font-size: 14px; font-weight: bold;");
                refresh();
            });
        }
    });'''

text = pattern.sub(new_connections, text)

with open('src/TablePages.cpp', 'w') as f:
    f.write(text)
