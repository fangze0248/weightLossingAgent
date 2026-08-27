#include "modeljsoncodec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {

QJsonArray ingredientsToArray(const QVector<Ingredient>& ingredients)
{
    QJsonArray array;
    for (const Ingredient& ingredient : ingredients) {
        QJsonObject object;
        object.insert(QStringLiteral("name"), ingredient.name);
        object.insert(QStringLiteral("amount"), ingredient.amount);
        object.insert(QStringLiteral("unit"), ingredient.unit);
        array.append(object);
    }
    return array;
}

QVector<Ingredient> ingredientsFromArray(const QJsonArray& array)
{
    QVector<Ingredient> ingredients;
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        Ingredient ingredient;
        ingredient.name = object.value(QStringLiteral("name")).toString();
        ingredient.amount = object.value(QStringLiteral("amount")).toDouble();
        ingredient.unit = object.value(QStringLiteral("unit")).toString();
        ingredients.append(ingredient);
    }
    return ingredients;
}

QJsonArray stringListToArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        array.append(value);
    }
    return array;
}

QStringList stringListFromArray(const QJsonArray& array)
{
    QStringList values;
    for (const QJsonValue& value : array) {
        if (value.isString()) {
            values.append(value.toString());
        }
    }
    return values;
}

QJsonObject calorieNeedToObject(const CalorieNeed& value)
{
    QJsonObject object;
    object.insert(QStringLiteral("bmi"), value.bmi);
    object.insert(QStringLiteral("bmiEvaluation"), value.bmiEvaluation);
    object.insert(QStringLiteral("bmr"), value.bmr);
    object.insert(QStringLiteral("tdee"), value.tdee);
    object.insert(QStringLiteral("dailyDeficit"), value.dailyDeficit);
    object.insert(QStringLiteral("dietDeficit"), value.dietDeficit);
    object.insert(QStringLiteral("recommendedIntake"), value.recommendedIntake);
    object.insert(QStringLiteral("exerciseTarget"), value.exerciseTarget);
    return object;
}

CalorieNeed calorieNeedFromObject(const QJsonObject& object)
{
    CalorieNeed value;
    value.bmi = object.value(QStringLiteral("bmi")).toDouble();
    value.bmiEvaluation = object.value(QStringLiteral("bmiEvaluation")).toString();
    value.bmr = object.value(QStringLiteral("bmr")).toDouble();
    value.tdee = object.value(QStringLiteral("tdee")).toDouble();
    value.dailyDeficit = object.value(QStringLiteral("dailyDeficit")).toDouble();
    value.dietDeficit = object.value(QStringLiteral("dietDeficit")).toDouble();
    value.recommendedIntake = object.value(QStringLiteral("recommendedIntake")).toDouble();
    value.exerciseTarget = object.value(QStringLiteral("exerciseTarget")).toDouble();
    return value;
}

QJsonObject exercisePlanItemToObject(const ExercisePlanItem& value)
{
    QJsonObject object;
    object.insert(QStringLiteral("exerciseId"), value.exerciseId);
    object.insert(QStringLiteral("exerciseName"), value.exerciseName);
    object.insert(QStringLiteral("durationMinutes"), value.durationMinutes);
    object.insert(QStringLiteral("caloriesBurned"), value.caloriesBurned);
    return object;
}

ExercisePlanItem exercisePlanItemFromObject(const QJsonObject& object)
{
    ExercisePlanItem value;
    value.exerciseId = object.value(QStringLiteral("exerciseId")).toString();
    value.exerciseName = object.value(QStringLiteral("exerciseName")).toString();
    value.durationMinutes = object.value(QStringLiteral("durationMinutes")).toInt();
    value.caloriesBurned = object.value(QStringLiteral("caloriesBurned")).toDouble();
    return value;
}

QJsonObject mealPlanItemToObject(const MealPlanItem& value)
{
    QJsonObject object;
    object.insert(QStringLiteral("recipeId"), value.recipeId);
    object.insert(QStringLiteral("recipeName"), value.recipeName);
    object.insert(QStringLiteral("mealType"), toStorageString(value.mealType));
    object.insert(QStringLiteral("ingredients"), ingredientsToArray(value.ingredients));
    object.insert(QStringLiteral("nutritionTags"), stringListToArray(value.nutritionTags));
    object.insert(QStringLiteral("calories"), value.calories);
    return object;
}

MealPlanItem mealPlanItemFromObject(const QJsonObject& object)
{
    MealPlanItem value;
    value.recipeId = object.value(QStringLiteral("recipeId")).toString();
    value.recipeName = object.value(QStringLiteral("recipeName")).toString();
    value.mealType = mealTypeFromStorageString(
                         object.value(QStringLiteral("mealType")).toString())
                         .value_or(MealType::Breakfast);
    value.ingredients = ingredientsFromArray(
        object.value(QStringLiteral("ingredients")).toArray());
    value.nutritionTags = stringListFromArray(
        object.value(QStringLiteral("nutritionTags")).toArray());
    value.calories = object.value(QStringLiteral("calories")).toDouble();
    return value;
}

QJsonArray exerciseItemsToArray(const QVector<ExercisePlanItem>& items)
{
    QJsonArray array;
    for (const ExercisePlanItem& item : items) {
        array.append(exercisePlanItemToObject(item));
    }
    return array;
}

QVector<ExercisePlanItem> exerciseItemsFromArray(const QJsonArray& array)
{
    QVector<ExercisePlanItem> items;
    for (const QJsonValue& value : array) {
        if (value.isObject()) {
            items.append(exercisePlanItemFromObject(value.toObject()));
        }
    }
    return items;
}

QJsonArray mealItemsToArray(const QVector<MealPlanItem>& items)
{
    QJsonArray array;
    for (const MealPlanItem& item : items) {
        array.append(mealPlanItemToObject(item));
    }
    return array;
}

QVector<MealPlanItem> mealItemsFromArray(const QJsonArray& array)
{
    QVector<MealPlanItem> items;
    for (const QJsonValue& value : array) {
        if (value.isObject()) {
            items.append(mealPlanItemFromObject(value.toObject()));
        }
    }
    return items;
}

QJsonObject mealPlanToObject(const MealPlan& value)
{
    QJsonObject object;
    object.insert(QStringLiteral("breakfast"), mealItemsToArray(value.breakfast));
    object.insert(QStringLiteral("lunch"), mealItemsToArray(value.lunch));
    object.insert(QStringLiteral("dinner"), mealItemsToArray(value.dinner));
    object.insert(QStringLiteral("snacks"), mealItemsToArray(value.snacks));
    object.insert(QStringLiteral("totalCalories"), value.totalCalories);
    return object;
}

MealPlan mealPlanFromObject(const QJsonObject& object)
{
    MealPlan value;
    value.breakfast = mealItemsFromArray(object.value(QStringLiteral("breakfast")).toArray());
    value.lunch = mealItemsFromArray(object.value(QStringLiteral("lunch")).toArray());
    value.dinner = mealItemsFromArray(object.value(QStringLiteral("dinner")).toArray());
    value.snacks = mealItemsFromArray(object.value(QStringLiteral("snacks")).toArray());
    value.totalCalories = object.value(QStringLiteral("totalCalories")).toDouble();
    return value;
}

QJsonObject dailyPlanToObject(const DailyPlan& value)
{
    QJsonObject object;
    object.insert(QStringLiteral("date"), value.date.toString(Qt::ISODate));
    object.insert(QStringLiteral("calorieNeed"), calorieNeedToObject(value.calorieNeed));
    object.insert(QStringLiteral("exercises"), exerciseItemsToArray(value.exercises));
    object.insert(QStringLiteral("meals"), mealPlanToObject(value.meals));
    object.insert(QStringLiteral("totalCaloriesBurned"), value.totalCaloriesBurned);
    object.insert(QStringLiteral("completed"), value.completed);
    return object;
}

DailyPlan dailyPlanFromObject(const QJsonObject& object)
{
    DailyPlan value;
    value.date = QDate::fromString(object.value(QStringLiteral("date")).toString(), Qt::ISODate);
    value.calorieNeed = calorieNeedFromObject(
        object.value(QStringLiteral("calorieNeed")).toObject());
    value.exercises = exerciseItemsFromArray(
        object.value(QStringLiteral("exercises")).toArray());
    value.meals = mealPlanFromObject(object.value(QStringLiteral("meals")).toObject());
    value.totalCaloriesBurned = object.value(QStringLiteral("totalCaloriesBurned")).toDouble();
    value.completed = object.value(QStringLiteral("completed")).toBool();
    return value;
}

} // namespace

namespace model_json_codec {

QString stringListToJson(const QStringList& values)
{
    return QString::fromUtf8(
        QJsonDocument(stringListToArray(values)).toJson(QJsonDocument::Compact));
}

QStringList stringListFromJson(const QString& json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    return document.isArray() ? stringListFromArray(document.array()) : QStringList{};
}

QString ingredientsToJson(const QVector<Ingredient>& ingredients)
{
    return QString::fromUtf8(
        QJsonDocument(ingredientsToArray(ingredients)).toJson(QJsonDocument::Compact));
}

QVector<Ingredient> ingredientsFromJson(const QString& json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    return document.isArray() ? ingredientsFromArray(document.array())
                              : QVector<Ingredient>{};
}

QString weeklyPlanToJson(const WeeklyPlan& plan)
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), plan.schemaVersion);
    root.insert(QStringLiteral("planId"), plan.planId);
    root.insert(QStringLiteral("userId"), plan.userId);
    root.insert(QStringLiteral("startDate"), plan.startDate.toString(Qt::ISODate));
    root.insert(QStringLiteral("generatedAt"), plan.generatedAt.toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("totalCaloriesIn"), plan.totalCaloriesIn);
    root.insert(QStringLiteral("totalCaloriesOut"), plan.totalCaloriesOut);

    QJsonArray days;
    for (const DailyPlan& day : plan.days) {
        days.append(dailyPlanToObject(day));
    }
    root.insert(QStringLiteral("days"), days);

    return QString::fromUtf8(
        QJsonDocument(root).toJson(QJsonDocument::Compact));
}

std::optional<WeeklyPlan> weeklyPlanFromJson(
    const QString& json,
    QString* errorMessage)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        json.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = parseError.errorString();
        }
        return std::nullopt;
    }

    const QJsonObject root = document.object();
    WeeklyPlan plan;
    plan.schemaVersion = root.value(QStringLiteral("schemaVersion")).toString(
        QStringLiteral("1.0"));
    plan.planId = root.value(QStringLiteral("planId")).toString();
    plan.userId = root.value(QStringLiteral("userId")).toString();
    plan.startDate = QDate::fromString(
        root.value(QStringLiteral("startDate")).toString(), Qt::ISODate);
    plan.generatedAt = QDateTime::fromString(
        root.value(QStringLiteral("generatedAt")).toString(), Qt::ISODateWithMs);
    if (!plan.generatedAt.isValid()) {
        plan.generatedAt = QDateTime::fromString(
            root.value(QStringLiteral("generatedAt")).toString(), Qt::ISODate);
    }
    plan.totalCaloriesIn = root.value(QStringLiteral("totalCaloriesIn")).toDouble();
    plan.totalCaloriesOut = root.value(QStringLiteral("totalCaloriesOut")).toDouble();

    const QJsonArray days = root.value(QStringLiteral("days")).toArray();
    for (const QJsonValue& value : days) {
        if (value.isObject()) {
            plan.days.append(dailyPlanFromObject(value.toObject()));
        }
    }

    return plan;
}

} // namespace model_json_codec
