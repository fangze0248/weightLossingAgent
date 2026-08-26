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
    std::optional<double> minimumCalories;
    std::optional<double> maximumCalories;
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
