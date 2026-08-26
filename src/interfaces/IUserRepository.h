#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/UserProfile.h"

#include <QString>
#include <QVector>
#include <optional>

class IUserRepository {
public:
    virtual ~IUserRepository() = default;

    virtual ServiceResult<QVector<UserProfile>> findAll() const = 0;
    virtual ServiceResult<std::optional<UserProfile>> findById(
        const QString& id) const = 0;
    virtual ServiceResult<UserProfile> add(const UserProfile& user) = 0;
    virtual ServiceResult<UserProfile> update(const UserProfile& user) = 0;
    virtual ServiceResult<bool> remove(const QString& id) = 0;
};
