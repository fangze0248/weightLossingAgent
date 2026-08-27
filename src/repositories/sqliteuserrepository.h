#pragma once

#include "interfaces/IUserRepository.h"

#include <QSqlDatabase>

class SqliteUserRepository final : public IUserRepository
{
public:
    explicit SqliteUserRepository(QSqlDatabase database);

    ServiceResult<QVector<UserProfile>> findAll() const override;
    ServiceResult<std::optional<UserProfile>> findById(
        const QString& id) const override;
    ServiceResult<UserProfile> add(const UserProfile& user) override;
    ServiceResult<UserProfile> update(const UserProfile& user) override;
    ServiceResult<bool> remove(const QString& id) override;

private:
    QSqlDatabase database_;
};
