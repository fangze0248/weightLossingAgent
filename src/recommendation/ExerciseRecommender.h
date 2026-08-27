#pragma once

#include "interfaces/IExerciseRecommender.h"

/**
 * @brief 基于规则生成运动处方的具体实现。
 *
 * 该类实现公共接口 IExerciseRecommender。当前阶段先负责输入检查，
 * 后续步骤会继续加入候选过滤、热量计算和运动组合搜索。
 */
class ExerciseRecommender final : public IExerciseRecommender
{
public:
    ServiceResult<QVector<ExercisePlanItem>> generate(
        const UserProfile& user,
        double targetCalories,
        const QVector<Exercise>& exerciseDatabase,
        const ExerciseRecommendationOptions& options = {}) const override;
};
