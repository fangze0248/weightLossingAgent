#pragma once

#include "../contracts/ServiceResult.h"
#include "../interfaces/IWeeklyPlanner.h"
#include "../models/PlanModels.h"

#include <QDate>
#include <QString>

class IPlanGenerationService {
public:
    virtual ~IPlanGenerationService() = default;

    virtual ServiceResult<WeeklyPlan> generateAndSave(
        const QString& userId,
        const QDate& startDate,
        const WeeklyPlanOptions& options = {}) = 0;
};

