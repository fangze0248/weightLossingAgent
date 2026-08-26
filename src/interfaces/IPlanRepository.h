#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/PlanModels.h"

#include <QString>
#include <QVector>
#include <optional>

class IPlanRepository {
public:
    virtual ~IPlanRepository() = default;

    virtual ServiceResult<WeeklyPlan> save(const WeeklyPlan& plan) = 0;
    virtual ServiceResult<std::optional<WeeklyPlan>> findById(
        const QString& planId) const = 0;
    virtual ServiceResult<QVector<WeeklyPlan>> findByUserId(
        const QString& userId) const = 0;
    virtual ServiceResult<bool> remove(const QString& planId) = 0;
};
