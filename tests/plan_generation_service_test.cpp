#include "application/plangenerationservice.h"
#include "database/databasemanager.h"
#include "recommendation/WeeklyPlanner.h"
#include "recommendation/healthcalculator.h"
#include "repositories/sqliteexerciserepository.h"
#include "repositories/sqlitefeedbackrepository.h"
#include "repositories/sqliteplanrepository.h"
#include "repositories/sqlitereciperepository.h"
#include "repositories/sqliteuserrepository.h"
#include "services/FeedbackService.h"

#include <QCoreApplication>
#include <QDate>
#include <QSet>
#include <QTemporaryDir>

#include <cstdio>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    DatabaseManager manager(directory.filePath(QStringLiteral("service.db")));
    QString error;
    if (!manager.open(&error)
        || !manager.initialize(&error)
        || !manager.seedDemoData(&error)) {
        return 2;
    }

    SqliteUserRepository users(manager.database());
    SqliteExerciseRepository exercises(manager.database());
    SqliteRecipeRepository recipes(manager.database());
    SqlitePlanRepository plans(manager.database());
    SqliteFeedbackRepository feedbacks(manager.database());
    FeedbackService feedbackService(feedbacks);
    HealthCalculator healthCalculator;
    WeeklyPlanner weeklyPlanner;
    PlanGenerationService service(users,
                                  exercises,
                                  recipes,
                                  plans,
                                  healthCalculator,
                                  weeklyPlanner,
                                  feedbackService);

    // 为三餐补充足够多的等质量候选，使跨周降重可以在不牺牲热量
    // 约束的前提下选择新菜。
    const auto addRecipes = [&recipes](const QString& prefix,
                                       MealType mealType,
                                       double calories) {
        for (int index = 0; index < 24; ++index) {
            Recipe recipe;
            recipe.id = QStringLiteral("history-%1-%2")
                            .arg(prefix)
                            .arg(index);
            recipe.name = recipe.id;
            recipe.mealType = mealType;
            recipe.totalCalories = calories;
            recipe.nutritionPerServing.caloriesKcal = calories;
            if (!recipes.add(recipe).ok) return false;
        }
        return true;
    };
    if (!addRecipes(
            QStringLiteral("breakfast"),
            MealType::Breakfast,
            520.0)
        || !addRecipes(
            QStringLiteral("lunch"),
            MealType::Lunch,
            700.0)
        || !addRecipes(
            QStringLiteral("dinner"),
            MealType::Dinner,
            520.0)) {
        return 6;
    }

    // 补充同质量运动候选，以验证跨周运动历史降权。
    for (int index = 0; index < 24; ++index) {
        Exercise exercise;
        exercise.id = QStringLiteral("history-exercise-%1").arg(index);
        exercise.name = exercise.id;
        exercise.metValue = 4.0;
        exercise.category = ExerciseCategory::Aerobic;
        if (!exercises.add(exercise).ok) return 9;
    }

    WeeklyPlanOptions generationOptions;
    generationOptions.randomSeed = 20260902;
    generationOptions.dailyTargetVariationRatio = 0.0;
    const auto result = service.generateAndSave(
        QStringLiteral("U001"),
        QDate(2026, 8, 24),
        generationOptions);
    if (!result.ok) {
        std::fprintf(stderr,
                     "%s: %s\n",
                     result.code.toUtf8().constData(),
                     result.message.toUtf8().constData());
        return 30;
    }
    if (result.data.days.size() != 7) return 31;
    for (const DailyPlan& day : result.data.days) {
        if (!day.meals.snacks.isEmpty()) return 32;
    }

    const auto firstSavedPlans = plans.findByUserId(QStringLiteral("U001"));
    if (!firstSavedPlans.ok || firstSavedPlans.data.size() != 1) return 4;
    if (firstSavedPlans.data.first().planId != result.data.planId) return 5;

    const auto recipeIdsOf = [](const WeeklyPlan& plan) {
        QSet<QString> ids;
        const auto appendItems = [&ids](const QVector<MealPlanItem>& items) {
            for (const MealPlanItem& item : items) ids.insert(item.recipeId);
        };
        for (const DailyPlan& day : plan.days) {
            appendItems(day.meals.breakfast);
            appendItems(day.meals.lunch);
            appendItems(day.meals.dinner);
            appendItems(day.meals.snacks);
        }
        return ids;
    };
    const auto exerciseIdsOf = [](const WeeklyPlan& plan) {
        QSet<QString> ids;
        for (const DailyPlan& day : plan.days) {
            for (const ExercisePlanItem& item : day.exercises) {
                ids.insert(item.exerciseId);
            }
        }
        return ids;
    };

    const auto secondResult = service.generateAndSave(
        QStringLiteral("U001"),
        QDate(2026, 8, 31),
        generationOptions);
    if (!secondResult.ok) {
        std::fprintf(stderr,
                     "%s: %s\n",
                     secondResult.code.toUtf8().constData(),
                     secondResult.message.toUtf8().constData());
        return 7;
    }
    const QSet<QString> firstWeekIds = recipeIdsOf(result.data);
    const QSet<QString> secondWeekIds = recipeIdsOf(secondResult.data);
    QSet<QString> overlap = firstWeekIds;
    overlap.intersect(secondWeekIds);
    if (firstWeekIds.isEmpty()
        || secondWeekIds.isEmpty()
        || firstWeekIds == secondWeekIds
        || overlap.size() >= firstWeekIds.size()) {
        return 8;
    }
    const QSet<QString> firstWeekExerciseIds = exerciseIdsOf(result.data);
    const QSet<QString> secondWeekExerciseIds = exerciseIdsOf(secondResult.data);
    QSet<QString> exerciseOverlap = firstWeekExerciseIds;
    exerciseOverlap.intersect(secondWeekExerciseIds);
    if (firstWeekExerciseIds.isEmpty()
        || secondWeekExerciseIds.isEmpty()
        || firstWeekExerciseIds == secondWeekExerciseIds
        || exerciseOverlap.size() >= firstWeekExerciseIds.size()) {
        return 10;
    }

    Recipe snack;
    snack.id = QStringLiteral("R_SNACK_TEST");
    snack.name = QStringLiteral("测试加餐");
    snack.totalCalories = 180.0;
    snack.nutritionPerServing.caloriesKcal = 180.0;
    snack.nutritionPerServing.proteinG = 8.0;
    snack.nutritionPerServing.carbohydrateG = 20.0;
    snack.nutritionPerServing.fatG = 7.0;
    snack.servings = 1;
    snack.mealType = MealType::Snack;
    if (!recipes.add(snack).ok) return 40;

    const auto userResult = users.findById(QStringLiteral("U001"));
    if (!userResult.ok || !userResult.data.has_value()) return 41;
    UserProfile user = *userResult.data;
    user.includeSnack = true;
    if (!users.update(user).ok) return 42;

    const auto snackResult = service.generateAndSave(
        QStringLiteral("U001"),
        QDate(2026, 9, 7),
        generationOptions);
    if (!snackResult.ok || snackResult.data.days.size() != 7) return 43;
    for (const DailyPlan& day : snackResult.data.days) {
        if (day.meals.snacks.isEmpty()) return 44;
    }

    const auto savedPlans = plans.findByUserId(QStringLiteral("U001"));
    if (!savedPlans.ok || savedPlans.data.size() != 3) return 45;
    return 0;
}

