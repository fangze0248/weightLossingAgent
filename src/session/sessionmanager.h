#pragma once

#include <QObject>
#include <QString>

class SessionManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject* parent = nullptr);

    bool hasCurrentUser() const;
    QString currentUserId() const;
    void setCurrentUserId(const QString& userId);
    void clearCurrentUser();

signals:
    void currentUserChanged(const QString& userId);

private:
    QString currentUserId_;
};
