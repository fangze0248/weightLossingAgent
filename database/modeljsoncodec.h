#pragma once

#include "models/PlanModels.h"
#include "models/Recipe.h"

#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace model_json_codec {

QString stringListToJson(const QStringList& values);
QStringList stringListFromJson(const QString& json);

QString ingredientsToJson(const QVector<Ingredient>& ingredients);
QVector<Ingredient> ingredientsFromJson(const QString& json);

QString nutritionFactsToJson(const NutritionFacts& nutrition);
NutritionFacts nutritionFactsFromJson(const QString& json);

QString weeklyPlanToJson(const WeeklyPlan& plan);
std::optional<WeeklyPlan> weeklyPlanFromJson(
    const QString& json,
    QString* errorMessage = nullptr);

} // namespace model_json_codec
