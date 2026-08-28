#include "application/plangenerationservice.h"

#include "interfaces/IExerciseRepository.h"
#include "interfaces/IHealthCalculator.h"
#include "interfaces/IPlanRepository.h"
#include "interfaces/IRecipeRepository.h"
#include "interfaces/IUserRepository.h"
#include "interfaces/IWeeklyPlanner.h"

#include <utility>

PlanGenerationService::PlanGenerationService(
    IUserRepository& userRepository,
    IExerciseRepository& exerciseRepository,
    IRecipeRepository& recipeRepository,
    IPlanRepository& planRepository,
    IHealthCalculator& healthCalculator,
    IWeeklyPlanner& weeklyPlanner)
    : userRepository_(userRepository),
      exerciseRepository_(exerciseRepository),
      recipeRepository_(recipeRepository),
      planRepository_(planRepository),
      healthCalculator_(healthCalculator),
      weeklyPlanner_(weeklyPlanner)
{
}

ServiceResult<WeeklyPlan> PlanGenerationService::generateAndSave(
    const QString& userId,
    const QDate& startDate,
    const WeeklyPlanOptions& options)
{
    const QString normalizedUserId = userId.trimmed();
    if (normalizedUserId.isEmpty() || !startDate.isValid()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("INVALID_GENERATION_REQUEST"),
            QStringLiteral("用户编号和计划开始日期不能为空。"));
    }

    const auto userResult = userRepository_.findById(normalizedUserId);
    if (!userResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            userResult.code, userResult.message, userResult.warnings);
    }
    if (!userResult.data.has_value()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("USER_NOT_FOUND"),
            QStringLiteral("当前登录用户不存在。"));
    }

    const auto calorieResult = healthCalculator_.calculate(*userResult.data);
    if (!calorieResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            calorieResult.code,
            calorieResult.message,
            calorieResult.warnings);
    }

    const auto exerciseResult = exerciseRepository_.findAll();
    if (!exerciseResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            exerciseResult.code,
            exerciseResult.message,
            exerciseResult.warnings);
    }

    const auto recipeResult = recipeRepository_.findAll();
    if (!recipeResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            recipeResult.code,
            recipeResult.message,
            recipeResult.warnings);
    }

    auto planResult = weeklyPlanner_.generate(
        *userResult.data,
        calorieResult.data,
        startDate,
        exerciseResult.data,
        recipeResult.data,
        options);
    planResult.warnings.append(calorieResult.warnings);
    if (!planResult.ok) {
        return planResult;
    }

    const auto saveResult = planRepository_.save(planResult.data);
    if (!saveResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            saveResult.code,
            saveResult.message,
            saveResult.warnings);
    }

    QStringList warnings = planResult.warnings;
    warnings.append(saveResult.warnings);
    return ServiceResult<WeeklyPlan>::success(
        saveResult.data,
        QStringLiteral("周计划已生成并保存。"),
        std::move(warnings));
}

