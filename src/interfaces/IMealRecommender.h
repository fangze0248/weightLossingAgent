#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/PlanModels.h"
#include "../models/Recipe.h"
#include "../models/UserProfile.h"

#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <optional>

struct MealRecommendationOptions {
    QStringList excludedRecipeIds;
    double toleranceRatio = 0.10;
    double breakfastRatio = 0.30;
    double lunchRatio = 0.40;
    double dinnerRatio = 0.30;
    double snackRatio = 0.0;
    int maximumItemsPerMeal = 2;
    bool includeSnack = false;
    std::optional<quint32> randomSeed;
};

class IMealRecommender {
public:
    virtual ~IMealRecommender() = default;

    virtual ServiceResult<MealPlan> generate(
        const UserProfile& user,
        double targetCalories,
        const QVector<Recipe>& recipeDatabase,
        const MealRecommendationOptions& options = {}) const = 0;
};
