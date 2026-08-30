#pragma once

#include "interfaces/IWeeklyPlanner.h"

/**
 * 协调运动推荐与食谱推荐，生成固定七天的周计划。
 */
class WeeklyPlanner final : public IWeeklyPlanner
{
public:
    ServiceResult<WeeklyPlan> generate(
        const UserProfile& user,
        const CalorieNeed& calorieNeed,
        const QDate& startDate,
        const QVector<Exercise>& exerciseDatabase,
        const QVector<Recipe>& recipeDatabase,
        const WeeklyPlanOptions& options = {}) const override;
};
