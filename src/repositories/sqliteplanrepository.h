#pragma once

#include "interfaces/IPlanRepository.h"

#include <QSqlDatabase>

class SqlitePlanRepository final : public IPlanRepository
{
public:
    explicit SqlitePlanRepository(QSqlDatabase database);

    ServiceResult<WeeklyPlan> save(const WeeklyPlan& plan) override;
    ServiceResult<std::optional<WeeklyPlan>> findById(
        const QString& planId) const override;
    ServiceResult<QVector<WeeklyPlan>> findByUserId(
        const QString& userId) const override;
    ServiceResult<bool> remove(const QString& planId) override;

private:
    QSqlDatabase database_;
};
