#include "database/databasemanager.h"
#include "recommendation/WeeklyPlanner.h"
#include "repositories/sqliteexerciseRepository.h"
#include "repositories/sqliteplanrepository.h"
#include "repositories/sqlitereciperepository.h"
#include "repositories/sqliteuserrepository.h"

#include <QCoreApplication>
#include <QDate>
#include <QTemporaryDir>

#include <cmath>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;

    DatabaseManager manager(
        temporaryDirectory.filePath(QStringLiteral("integration_test.db")));
    QString databaseError;
    if (!manager.open(&databaseError)) return 2;
    if (!manager.initialize(&databaseError)) return 3;
    if (!manager.seedDemoData(&databaseError)) return 4;

    SqliteUserRepository userRepository(manager.database());
    SqliteExerciseRepository exerciseRepository(manager.database());
    SqliteRecipeRepository recipeRepository(manager.database());
    SqlitePlanRepository planRepository(manager.database());

    const auto userResult = userRepository.findById(QStringLiteral("U001"));
    const auto exerciseResult = exerciseRepository.findAll();
    const auto recipeResult = recipeRepository.findAll();

    if (!userResult.ok || !userResult.data.has_value()) return 5;
    if (!exerciseResult.ok || exerciseResult.data.isEmpty()) return 6;
    if (!recipeResult.ok || recipeResult.data.isEmpty()) return 7;

    // 演示数据三餐合计为 1560 千卡；80 kg 用户慢跑10分钟约消耗98千卡。
    // 这两个目标都能由演示数据库精确或在10%容差内满足。
    CalorieNeed calorieNeed;
    calorieNeed.recommendedIntake = 1560.0;
    calorieNeed.exerciseTarget = 98.0;

    const QDate startDate(2026, 8, 27);
    WeeklyPlanOptions options;

    WeeklyPlanner planner;
    const auto weeklyResult = planner.generate(
        *userResult.data,
        calorieNeed,
        startDate,
        exerciseResult.data,
        recipeResult.data,
        options);

    if (!weeklyResult.ok || weeklyResult.data.days.size() != 7) return 8;
    if (weeklyResult.data.userId != userResult.data->id) return 9;
    double calculatedCaloriesIn = 0.0;
    for (const DailyPlan& day : weeklyResult.data.days) {
        if (day.exercises.isEmpty()
            || day.meals.breakfast.isEmpty()
            || day.meals.lunch.isEmpty()
            || day.meals.dinner.isEmpty()) {
            return 11;
        }
        calculatedCaloriesIn += day.meals.totalCalories;
    }
    if (std::abs(weeklyResult.data.totalCaloriesIn - calculatedCaloriesIn)
        > 1e-9) {
        return 10;
    }

    const auto saveResult = planRepository.save(weeklyResult.data);
    if (!saveResult.ok) return 12;

    const auto loadedResult = planRepository.findById(
        weeklyResult.data.planId);
    if (!loadedResult.ok || !loadedResult.data.has_value()) return 13;

    const WeeklyPlan& loaded = *loadedResult.data;
    if (loaded.planId != weeklyResult.data.planId
        || loaded.userId != weeklyResult.data.userId
        || loaded.startDate != weeklyResult.data.startDate
        || loaded.days.size() != 7
        || std::abs(
               loaded.totalCaloriesIn - weeklyResult.data.totalCaloriesIn)
            > 1e-9
        || std::abs(
               loaded.totalCaloriesOut - weeklyResult.data.totalCaloriesOut)
            > 1e-9) {
        return 14;
    }

    return 0;
}
