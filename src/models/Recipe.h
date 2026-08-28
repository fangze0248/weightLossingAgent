#pragma once

#include "DomainEnums.h"
#include "NutritionFacts.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct Ingredient
{
    QString name;
    double amount = 0.0;
    QString unit;
};

struct Recipe
{
    QString id;
    QString name;
    QVector<Ingredient> ingredients;

    // 暂时保留，保证已有代码继续工作
    double totalCalories = 0.0;

    // 新增：每一份食谱的详细营养
    NutritionFacts nutritionPerServing;

    // 新增：这份食谱可以供几人食用
    int servings = 1;

    MealType mealType = MealType::Breakfast;
    QStringList nutritionTags;
};