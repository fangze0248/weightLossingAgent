#include "ui/loginpage.h"

#include "interfaces/IUserRepository.h"
#include "session/sessionmanager.h"
#include "ui/profiledialog.h"

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

LoginPage::LoginPage(IUserRepository& repository,
                     SessionManager& sessionManager,
                     QWidget* parent)
    : QWidget(parent),
      repository_(repository),
      sessionManager_(sessionManager)
{
    setObjectName(QStringLiteral("authPage"));

    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("authCard"));
    card->setFixedWidth(430);

    auto* logoLabel = new QLabel(QStringLiteral("⚖"), card);
    logoLabel->setObjectName(QStringLiteral("authLogo"));
    logoLabel->setAlignment(Qt::AlignCenter);
    auto* titleLabel = new QLabel(QStringLiteral("减重智能体"), card);
    titleLabel->setObjectName(QStringLiteral("authTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    auto* subtitleLabel = new QLabel(
        QStringLiteral("登录后查看个人健康数据、运动处方与食谱计划"),
        card);
    subtitleLabel->setProperty("role", "subtitle");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);

    userIdEdit_ = new QLineEdit(card);
    userIdEdit_->setPlaceholderText(QStringLiteral("输入用户编号，例如 U001"));
    userIdEdit_->setMinimumHeight(40);

    auto* loginButton = new QPushButton(QStringLiteral("登录"), card);
    loginButton->setProperty("variant", "primary");
    loginButton->setMinimumHeight(40);
    auto* createButton = new QPushButton(QStringLiteral("新建账号"), card);

    auto* dividerLabel = new QLabel(
        QStringLiteral("本地账号仅保存在本机 SQLite 数据库"), card);
    dividerLabel->setProperty("role", "subtitle");
    dividerLabel->setAlignment(Qt::AlignCenter);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(42, 34, 42, 34);
    cardLayout->setSpacing(14);
    cardLayout->addWidget(logoLabel);
    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(subtitleLabel);
    cardLayout->addSpacing(12);
    cardLayout->addWidget(userIdEdit_);
    cardLayout->addWidget(loginButton);
    cardLayout->addWidget(createButton);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(dividerLabel);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->addStretch();
    rootLayout->addWidget(card, 0, Qt::AlignHCenter);
    rootLayout->addStretch();

    connect(loginButton, &QPushButton::clicked, this, &LoginPage::login);
    connect(createButton, &QPushButton::clicked, this, &LoginPage::createAccount);
    connect(userIdEdit_, &QLineEdit::returnPressed, this, &LoginPage::login);
}

void LoginPage::prepareForDisplay()
{
    userIdEdit_->clear();
    userIdEdit_->setFocus();
}

void LoginPage::login()
{
    const QString userId = userIdEdit_->text().trimmed();
    if (userId.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("登录失败"),
                             QStringLiteral("请输入用户编号"));
        return;
    }

    const auto result = repository_.findById(userId);
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("登录失败"),
                             result.message);
        return;
    }
    if (!result.data.has_value()) {
        QMessageBox::warning(this,
                             QStringLiteral("登录失败"),
                             QStringLiteral("用户不存在，请先新建账号"));
        return;
    }

    sessionManager_.setCurrentUserId(result.data->id);
    emit loginSucceeded();
}

void LoginPage::createAccount()
{
    ProfileDialog dialog(repository_, ProfileDialog::Mode::Create, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    sessionManager_.setCurrentUserId(dialog.savedUser().id);
    emit loginSucceeded();
}
