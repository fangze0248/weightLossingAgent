#pragma once

#include "interfaces/IMealRecommender.h"

/**
 * 基于规则和热量区间生成每日食谱。
 *
 * 当前类直接实现公共接口，调用方只需要依赖 IMealRecommender，
 * 后续可以在不修改界面或 AppFacade 的情况下替换算法实现。
 */
class MealRecommender final : public IMealRecommender
{
public:
    ServiceResult<MealPlan> generate(
        const UserProfile& user,
        double targetCalories,
        const QVector<Recipe>& recipeDatabase,
        const MealRecommendationOptions& options = {}) const override;
};
