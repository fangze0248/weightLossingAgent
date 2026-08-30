#pragma once

#include "DomainEnums.h"
#include "Recipe.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

struct CalorieNeed {
    double bmi = 0.0;
    QString bmiEvaluation;
    double bmr = 0.0;
    double tdee = 0.0;
    double dailyDeficit = 0.0;
    double dietDeficit = 0.0;
    double recommendedIntake = 0.0;
    double exerciseTarget = 0.0;
};

struct ExercisePlanItem {
    QString exerciseId;
    QString exerciseName;
    int durationMinutes = 0;
    double caloriesBurned = 0.0;
};

struct MealPlanItem {
    QString recipeId;
    QString recipeName;
    MealType mealType = MealType::Breakfast;
    QVector<Ingredient> ingredients;
    QStringList nutritionTags;
    double calories = 0.0;
    NutritionFacts nutrition;
};

struct MealPlan {
    QVector<MealPlanItem> breakfast;
    QVector<MealPlanItem> lunch;
    QVector<MealPlanItem> dinner;
    QVector<MealPlanItem> snacks;
    double totalCalories = 0.0;
    NutritionFacts totalNutrition;
};

struct DailyPlan {
    QDate date;
    CalorieNeed calorieNeed;
    QVector<ExercisePlanItem> exercises;
    MealPlan meals;
    double totalCaloriesBurned = 0.0;
    bool completed = false;
};

struct WeeklyPlan {
    QString schemaVersion = QStringLiteral("1.0");
    QString planId;
    QString userId;
    QDate startDate;
    QDateTime generatedAt;
    QVector<DailyPlan> days;
    double totalCaloriesIn = 0.0;
    double totalCaloriesOut = 0.0;
};

struct DashboardData {
    QString userId;
    QString userName;
    CalorieNeed calorieNeed;
    WeeklyPlan currentWeeklyPlan;
    bool hasCurrentWeeklyPlan = false;
};
