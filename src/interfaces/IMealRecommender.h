#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/PlanModels.h"
#include "../models/RecommendationPreference.h"
#include "../models/Recipe.h"
#include "../models/UserProfile.h"

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <optional>

struct MealRecommendationOptions {
    QStringList excludedRecipeIds;
    // 近期周计划中的食谱 ID -> 暴露惩罚。数值越大，越优先探索
    // 质量相近的新食谱；这是软约束，不会直接删除候选。
    QHash<QString, double> recentRecipePenalties;
    double toleranceRatio = 0.10;
    double breakfastRatio = 0.30;
    double lunchRatio = 0.40;
    double dinnerRatio = 0.30;
    double snackRatio = 0.0;
    int maximumItemsPerMeal = 2;
    bool includeSnack = false;
    std::optional<quint32> randomSeed;
    RecommendationPreference preference;
    // 可选的全天三大营养素目标。未提供时，推荐逻辑保持只按热量与餐次比例排序。
    std::optional<NutritionFacts> nutritionTarget;
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
