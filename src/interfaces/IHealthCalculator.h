#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/PlanModels.h"
#include "../models/UserProfile.h"

class IHealthCalculator {
public:
    virtual ~IHealthCalculator() = default;

    virtual ServiceResult<CalorieNeed> calculate(
        const UserProfile& user) const = 0;
};
