#pragma once

#include <QWidget>

class IUserRepository;
class QLineEdit;
class SessionManager;

class LoginPage : public QWidget
{
    Q_OBJECT

public:
    explicit LoginPage(IUserRepository& repository,
                       SessionManager& sessionManager,
                       QWidget* parent = nullptr);

    void prepareForDisplay();

signals:
    void loginSucceeded();

private:
    void login();
    void createAccount();

    IUserRepository& repository_;
    SessionManager& sessionManager_;
    QLineEdit* userIdEdit_ = nullptr;
};
