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
