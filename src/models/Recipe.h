#pragma once

#include "DomainEnums.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct Ingredient {
    QString name;
    double amount = 0.0;
    QString unit;
};

struct Recipe {
    QString id;
    QString name;
    QVector<Ingredient> ingredients;
    double totalCalories = 0.0;
    MealType mealType = MealType::Breakfast;
    QStringList nutritionTags;
};
