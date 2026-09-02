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

    const auto savedPlans = plans.findByUserId(QStringLiteral("U001"));
    if (!savedPlans.ok || savedPlans.data.size() != 1) return 4;
    if (savedPlans.data.first().planId != result.data.planId) return 5;

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
    return 0;
}

