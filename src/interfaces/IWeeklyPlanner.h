#pragma once

#include "IExerciseRecommender.h"
#include "IMealRecommender.h"
#include "../contracts/ServiceResult.h"
#include "../models/Exercise.h"
#include "../models/PlanModels.h"
#include "../models/Recipe.h"
#include "../models/UserProfile.h"

#include <QDate>
#include <QVector>
#include <QtGlobal>
#include <optional>

struct WeeklyPlanOptions {
    int numberOfDays = 7;
    bool avoidConsecutiveDuplicateExercises = true;
    bool avoidConsecutiveDuplicateRecipes = true;
    std::optional<quint32> randomSeed;
    ExerciseRecommendationOptions exerciseOptions;
    MealRecommendationOptions mealOptions;
};

class IWeeklyPlanner {
public:
    virtual ~IWeeklyPlanner() = default;

    virtual ServiceResult<WeeklyPlan> generate(
        const UserProfile& user,
        const CalorieNeed& calorieNeed,
        const QDate& startDate,
        const QVector<Exercise>& exerciseDatabase,
        const QVector<Recipe>& recipeDatabase,
        const WeeklyPlanOptions& options = {}) const = 0;
};
