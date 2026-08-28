#pragma once

#include "interfaces/IHealthCalculator.h"

class HealthCalculator final : public IHealthCalculator
{
public:
    ServiceResult<CalorieNeed> calculate(
        const UserProfile& user) const override;
};
