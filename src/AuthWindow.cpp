#include "include/AuthWindow.h"
#include "include/AuthManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QProgressBar>
#include <QRegularExpression>

namespace Quiver {

struct AuthWindow::Impl {
    QStackedWidget* stack_ {};
    

    QLineEdit* login_id_ {};
    QLineEdit* login_pass_ {};
    QLabel* login_error_ {};

    
    QLineEdit* reg_first_ {};
    QLineEdit* reg_last_ {};
    QLineEdit* reg_user_ {};
    QLineEdit* reg_email_ {};
    QLineEdit* reg_pass_ {};
    QLineEdit* reg_confirm_ {};
    QProgressBar* pass_strength_ {};
};

AuthWindow::AuthWindow(QWidget* parent)
    : QDialog(parent), pimpl_{std::make_unique<Impl>()}
{
    setFixedSize(450, 600);
    setWindowTitle("Welcome to Quiver");
    setObjectName("CreateDialog"); 

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(40, 50, 40, 40);
    main_layout->setSpacing(20);


    auto* title = new QLabel("QUIVER");
    title->setObjectName("PageTitle");
    title->setAlignment(Qt::AlignCenter);
    main_layout->addWidget(title);

    pimpl_->stack_ = new QStackedWidget;

   
    auto* page_signin = new QWidget;
    auto* si_layout = new QVBoxLayout(page_signin);
    si_layout->setSpacing(15);

    auto* si_title = new QLabel("Sign In to your account");
    si_title->setStyleSheet("color: #A1A1AA; font-size: 14px;");
    si_title->setAlignment(Qt::AlignCenter);
    si_layout->addWidget(si_title);
    si_layout->addSpacing(10);

    pimpl_->login_id_ = new QLineEdit;
    pimpl_->login_id_->setPlaceholderText("Email or Username");
    pimpl_->login_id_->setFixedHeight(40);
    
    pimpl_->login_pass_ = new QLineEdit;
    pimpl_->login_pass_->setPlaceholderText("Password");
    pimpl_->login_pass_->setEchoMode(QLineEdit::Password);
    pimpl_->login_pass_->setFixedHeight(40);

    pimpl_->login_error_ = new QLabel("");
    pimpl_->login_error_->setStyleSheet("color: #fb7185; font-weight: bold; font-size: 12px;");
    pimpl_->login_error_->setAlignment(Qt::AlignCenter);

    auto* btn_forgot = new QPushButton("Forgot Password?");
    btn_forgot->setStyleSheet("color: #F97316; background: transparent; border: none; text-align: right; font-size: 12px;");
    btn_forgot->setCursor(Qt::PointingHandCursor);

    auto* btn_login = new QPushButton("Sign In");
    btn_login->setObjectName("PrimaryButton");
    btn_login->setFixedHeight(45);
    btn_login->setCursor(Qt::PointingHandCursor);
    connect(btn_login, &QPushButton::clicked, this, &AuthWindow::handle_login);

    auto* box_no_acc = new QHBoxLayout;
    auto* lbl_no_acc = new QLabel("Don't have an account?");
    lbl_no_acc->setStyleSheet("color: #71717A;");
    auto* btn_goto_reg = new QPushButton("Sign Up");
    btn_goto_reg->setStyleSheet("color: #FAFAFA; font-weight: bold; background: transparent; border: none;");
    btn_goto_reg->setCursor(Qt::PointingHandCursor);
    connect(btn_goto_reg, &QPushButton::clicked, this, [this](){ pimpl_->stack_->setCurrentIndex(1); });
    
    box_no_acc->addStretch();
    box_no_acc->addWidget(lbl_no_acc);
    box_no_acc->addWidget(btn_goto_reg);
    box_no_acc->addStretch();

    si_layout->addWidget(pimpl_->login_id_);
    si_layout->addWidget(pimpl_->login_pass_);
    si_layout->addWidget(btn_forgot);
    si_layout->addWidget(pimpl_->login_error_);
    si_layout->addStretch();
    si_layout->addWidget(btn_login);
    si_layout->addLayout(box_no_acc);
    
    auto* btn_guest = new QPushButton("Continue as Guest");
    btn_guest->setStyleSheet("color: #71717A; background: transparent; border: 1px solid #3F3F46; border-radius: 4px;");
    btn_guest->setFixedHeight(36);
    btn_guest->setCursor(Qt::PointingHandCursor);
    connect(btn_guest, &QPushButton::clicked, this, []() { AuthManager::get_instance().guest_login(); });
    si_layout->addWidget(btn_guest);
    
    pimpl_->stack_->addWidget(page_signin);

 
    auto* page_signup = new QWidget;
    auto* su_layout = new QVBoxLayout(page_signup);
    su_layout->setSpacing(12);

    auto* su_title = new QLabel("Create an Account");
    auto* reg_error = new QLabel("");
reg_error->setObjectName("reg_error_label");
reg_error->setStyleSheet("color: #fb7185; font-weight: bold; font-size: 12px;");
reg_error->setAlignment(Qt::AlignCenter);
    su_title->setStyleSheet("color: #A1A1AA; font-size: 14px;");
    su_title->setAlignment(Qt::AlignCenter);
    su_layout->addWidget(su_title);

    auto* name_row = new QHBoxLayout;
    pimpl_->reg_first_ = new QLineEdit; pimpl_->reg_first_->setPlaceholderText("First Name"); pimpl_->reg_first_->setFixedHeight(36);
    pimpl_->reg_last_  = new QLineEdit; pimpl_->reg_last_->setPlaceholderText("Last Name");   pimpl_->reg_last_->setFixedHeight(36);
    name_row->addWidget(pimpl_->reg_first_);
    name_row->addWidget(pimpl_->reg_last_);
    
    pimpl_->reg_user_ = new QLineEdit; pimpl_->reg_user_->setPlaceholderText("Username"); pimpl_->reg_user_->setFixedHeight(36);
    pimpl_->reg_email_ = new QLineEdit; pimpl_->reg_email_->setPlaceholderText("Email Address"); pimpl_->reg_email_->setFixedHeight(36);
    
    pimpl_->reg_pass_ = new QLineEdit; 
    pimpl_->reg_pass_->setPlaceholderText("Password"); 
    pimpl_->reg_pass_->setEchoMode(QLineEdit::Password); 
    pimpl_->reg_pass_->setFixedHeight(36);
    connect(pimpl_->reg_pass_, &QLineEdit::textChanged, this, &AuthWindow::evaluate_password_strength);

    pimpl_->pass_strength_ = new QProgressBar;
    pimpl_->pass_strength_->setFixedHeight(4);
    pimpl_->pass_strength_->setTextVisible(false);
    pimpl_->pass_strength_->setRange(0, 100);
    pimpl_->pass_strength_->setValue(0);
    pimpl_->pass_strength_->setStyleSheet("QProgressBar { background: #27272A; border: none; border-radius: 2px; } QProgressBar::chunk { background: #fb7185; border-radius: 2px; }");

    pimpl_->reg_confirm_ = new QLineEdit; 
    pimpl_->reg_confirm_->setPlaceholderText("Confirm Password"); 
    pimpl_->reg_confirm_->setEchoMode(QLineEdit::Password); 
    pimpl_->reg_confirm_->setFixedHeight(36);

    auto* btn_register = new QPushButton("Create Account");
    btn_register->setObjectName("PrimaryButton");
    btn_register->setFixedHeight(45);
    btn_register->setCursor(Qt::PointingHandCursor);
    connect(btn_register, &QPushButton::clicked, this, &AuthWindow::handle_register);

    auto* box_has_acc = new QHBoxLayout;
    auto* lbl_has_acc = new QLabel("Already have an account?");
    lbl_has_acc->setStyleSheet("color: #71717A;");
    auto* btn_goto_login = new QPushButton("Sign In");
    btn_goto_login->setStyleSheet("color: #FAFAFA; font-weight: bold; background: transparent; border: none;");
    btn_goto_login->setCursor(Qt::PointingHandCursor);
    connect(btn_goto_login, &QPushButton::clicked, this, [this](){ pimpl_->stack_->setCurrentIndex(0); });
    
    box_has_acc->addStretch();
    box_has_acc->addWidget(lbl_has_acc);
    box_has_acc->addWidget(btn_goto_login);
    box_has_acc->addStretch();

    su_layout->addLayout(name_row);
    su_layout->addWidget(pimpl_->reg_user_);
    su_layout->addWidget(pimpl_->reg_email_);
    su_layout->addWidget(pimpl_->reg_pass_);
    su_layout->addWidget(pimpl_->pass_strength_);
    su_layout->addWidget(pimpl_->reg_confirm_);
    su_layout->addStretch();
    su_layout->addWidget(btn_register);
    su_layout->addLayout(box_has_acc);

    pimpl_->stack_->addWidget(page_signup);

    main_layout->addWidget(pimpl_->stack_);


   connect(&AuthManager::get_instance(), &AuthManager::login_success, this, &QDialog::accept);connect(&AuthManager::get_instance(), &AuthManager::login_success, this, [this]() {
    AuthManager::get_instance().fetch_profile();
    accept();
});
connect(&AuthManager::get_instance(), &AuthManager::signup_success, this, [this]() {
    AuthManager::get_instance().fetch_profile();
    accept();
});connect(&AuthManager::get_instance(), &AuthManager::login_failed, this, [this](const QString& err){
    pimpl_->login_error_->setText(err);
});
connect(&AuthManager::get_instance(), &AuthManager::signup_success, this, &QDialog::accept);
connect(&AuthManager::get_instance(), &AuthManager::signup_failed, this, [this](const QString& err){
    pimpl_->login_error_->setText(err);  
});
}

AuthWindow::~AuthWindow() = default;

void AuthWindow::handle_login() {
    pimpl_->login_error_->setText("Authenticating...");
    AuthManager::get_instance().login(pimpl_->login_id_->text(), pimpl_->login_pass_->text());
}
void AuthWindow::handle_register() {
    if (pimpl_->reg_pass_->text() != pimpl_->reg_confirm_->text()) {
        pimpl_->login_error_->setText("Passwords do not match.");
        return;
    }
    AuthManager::get_instance().signUp(
        pimpl_->reg_first_->text(),
        pimpl_->reg_last_->text(),
        pimpl_->reg_user_->text(),
        pimpl_->reg_email_->text(),
        pimpl_->reg_pass_->text()
    );
}

void AuthWindow::evaluate_password_strength(const QString& password) {
    int score = 0;
    if (password.length() > 7) score += 25;
    if (password.contains(QRegularExpression("[A-Z]"))) score += 25;
    if (password.contains(QRegularExpression("[0-9]"))) score += 25;
    if (password.contains(QRegularExpression("[!@#$%^&*()_+]"))) score += 25;

    pimpl_->pass_strength_->setValue(score);


    QString color = "#fb7185";
    if (score >= 50) color = "#fbbf24"; 
    if (score >= 100) color = "#4ade80"; 

    pimpl_->pass_strength_->setStyleSheet(QString("QProgressBar { background: #27272A; border: none; border-radius: 2px; } QProgressBar::chunk { background: %1; border-radius: 2px; }").arg(color));
}

} 