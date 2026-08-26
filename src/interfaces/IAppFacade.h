#pragma once

#include "IDataExchangeService.h"
#include "IExerciseRepository.h"
#include "IMealRecommender.h"
#include "IRecipeRepository.h"
#include "IUserRepository.h"
#include "IWeeklyPlanner.h"
#include "../contracts/ServiceResult.h"
#include "../models/Feedback.h"
#include "../models/PlanModels.h"

#include <QDate>
#include <QString>
#include <QVector>
#include <optional>

class IAppFacade {
public:
    virtual ~IAppFacade() = default;

    virtual ServiceResult<QVector<UserProfile>> listUsers() const = 0;
    virtual ServiceResult<UserProfile> createUser(const UserProfile& user) = 0;
    virtual ServiceResult<UserProfile> updateUser(const UserProfile& user) = 0;
    virtual ServiceResult<bool> deleteUser(const QString& userId) = 0;

    virtual ServiceResult<QVector<Exercise>> listExercises(
        const ExerciseFilter& filter = {}) const = 0;
    virtual ServiceResult<Exercise> createExercise(const Exercise& exercise) = 0;
    virtual ServiceResult<Exercise> updateExercise(const Exercise& exercise) = 0;
    virtual ServiceResult<bool> deleteExercise(const QString& exerciseId) = 0;

    virtual ServiceResult<QVector<Recipe>> listRecipes(
        const RecipeFilter& filter = {}) const = 0;
    virtual ServiceResult<Recipe> createRecipe(const Recipe& recipe) = 0;
    virtual ServiceResult<Recipe> updateRecipe(const Recipe& recipe) = 0;
    virtual ServiceResult<bool> deleteRecipe(const QString& recipeId) = 0;

    virtual ServiceResult<DashboardData> getDashboard(
        const QString& userId) const = 0;
    virtual ServiceResult<WeeklyPlan> generateWeeklyPlan(
        const QString& userId,
        const QDate& startDate,
        const WeeklyPlanOptions& options = {}) = 0;
    virtual ServiceResult<std::optional<WeeklyPlan>> loadWeeklyPlan(
        const QString& planId) const = 0;
    virtual ServiceResult<QVector<WeeklyPlan>> listWeeklyPlans(
        const QString& userId) const = 0;
    virtual ServiceResult<bool> deleteWeeklyPlan(const QString& planId) = 0;

    virtual ServiceResult<Feedback> recordFeedback(
        const Feedback& feedback) = 0;
    virtual ServiceResult<bool> exportWeeklyPlan(
        const QString& planId,
        const QString& filePath,
        DataFormat format) const = 0;
};
