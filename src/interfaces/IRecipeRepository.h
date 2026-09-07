#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/Recipe.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

struct RecipeFilter {
    QString keyword;
    std::optional<MealType> mealType;
    QStringList requiredNutritionTags;
    QStringList excludedIds;
    std::optional<double> minimumCalories;
    std::optional<double> maximumCalories;
    std::optional<double> minimumProteinG;
    std::optional<double> maximumProteinG;
    std::optional<double> minimumCarbohydrateG;
    std::optional<double> maximumCarbohydrateG;
    std::optional<double> minimumFatG;
    std::optional<double> maximumFatG;
    std::optional<double> minimumFiberG;
    std::optional<double> maximumSodiumMg;
    std::optional<double> targetCalories;
    // 用于候选排序的餐次营养目标。三个字段应同时提供：先按热量
    // 接近程度排序，再按蛋白质、碳水和脂肪的平均相对偏差排序。
    std::optional<double> targetProteinG;
    std::optional<double> targetCarbohydrateG;
    std::optional<double> targetFatG;
    int limit = 0;
};

class IRecipeRepository {
public:
    virtual ~IRecipeRepository() = default;

    virtual ServiceResult<QVector<Recipe>> findAll(
        const RecipeFilter& filter = {}) const = 0;
    virtual ServiceResult<std::optional<Recipe>> findById(
        const QString& id) const = 0;
    virtual ServiceResult<Recipe> add(const Recipe& recipe) = 0;
    virtual ServiceResult<Recipe> update(const Recipe& recipe) = 0;
    virtual ServiceResult<bool> remove(const QString& id) = 0;
};
