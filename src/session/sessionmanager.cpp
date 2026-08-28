#include "session/sessionmanager.h"

#include <QSettings>

namespace {

const QString currentUserKey = QStringLiteral("session/currentUserId");

} // namespace

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
    QSettings settings;
    currentUserId_ = settings.value(currentUserKey).toString().trimmed();
}

bool SessionManager::hasCurrentUser() const
{
    return !currentUserId_.isEmpty();
}

QString SessionManager::currentUserId() const
{
    return currentUserId_;
}

void SessionManager::setCurrentUserId(const QString& userId)
{
    const QString normalizedId = userId.trimmed();
    if (normalizedId.isEmpty()) {
        clearCurrentUser();
        return;
    }

    QSettings settings;
    settings.setValue(currentUserKey, normalizedId);
    settings.sync();

    if (currentUserId_ == normalizedId) {
        return;
    }

    currentUserId_ = normalizedId;
    emit currentUserChanged(currentUserId_);
}

void SessionManager::clearCurrentUser()
{
    QSettings settings;
    settings.remove(currentUserKey);
    settings.sync();

    if (currentUserId_.isEmpty()) {
        return;
    }

    currentUserId_.clear();
    emit currentUserChanged(currentUserId_);
}
