#include "application/builtindatasetinitializer.h"
#include "application/csvdataexchangeservice.h"
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
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVector>

namespace {

struct TrialResult {
    qint64 firstStartupAndImportMs = 0;
    qint64 unchangedStartupMs = 0;
    qint64 weeklyPlanMs = 0;
    qint64 largeRecipeImportMs = 0;
    int recipeRows = 0;
};

double average(const QVector<TrialResult>& results,
               qint64 TrialResult::*member)
{
    qint64 total = 0;
    for (const TrialResult& result : results) total += result.*member;
    return results.isEmpty()
        ? 0.0
        : static_cast<double>(total) / results.size();
}

bool openAndInitialize(DatabaseManager& manager)
{
    QString error;
    if (manager.open(&error) && manager.initialize(&error)) return true;
    qCritical().noquote() << "Database setup failed:" << error;
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    const QDir sourceDirectory(QStringLiteral(PERFORMANCE_SOURCE_DIR));
    const QString exercisePath = sourceDirectory.filePath(
        QStringLiteral("datasets/builtin/exercises_zh.csv"));
    const QString recipePath = sourceDirectory.filePath(
        QStringLiteral("datasets/builtin/recipes_all_zh.csv"));
    if (!QFileInfo::exists(exercisePath) || !QFileInfo::exists(recipePath)) {
        qCritical().noquote() << "Benchmark datasets were not found."
                              << exercisePath << recipePath;
        return 1;
    }

    constexpr int trialCount = 3;
    QVector<TrialResult> results;
    results.reserve(trialCount);
    QTextStream output(stdout);

    for (int trial = 0; trial < trialCount; ++trial) {
        QTemporaryDir directory;
        if (!directory.isValid()) return 2;
        const QString databasePath = directory.filePath(
            QStringLiteral("performance.db"));

        TrialResult metrics;
        {
            QElapsedTimer startupTimer;
            startupTimer.start();

            DatabaseManager manager(databasePath);
            if (!openAndInitialize(manager)) return 3;
            SqliteUserRepository users(manager.database());
            SqliteExerciseRepository exercises(manager.database());
            SqliteRecipeRepository recipes(manager.database());
            SqlitePlanRepository plans(manager.database());
            SqliteFeedbackRepository feedback(manager.database());
            CsvDataExchangeService exchange;
            BuiltinDatasetInitializer initializer(
                manager.database(), exercises, recipes, exchange);

            const auto exerciseImport = initializer.importExercisesIfChanged(
                QStringLiteral("builtin_exercises"), exercisePath);
            if (!exerciseImport.ok || !exerciseImport.data.imported) {
                qCritical().noquote() << "Exercise import failed:"
                                      << exerciseImport.message;
                return 4;
            }

            QElapsedTimer recipeTimer;
            recipeTimer.start();
            const auto recipeImport = initializer.importRecipesIfChanged(
                QStringLiteral("builtin_recipes"), recipePath);
            metrics.largeRecipeImportMs = recipeTimer.elapsed();
            if (!recipeImport.ok || !recipeImport.data.imported) {
                qCritical().noquote() << "Recipe import failed:"
                                      << recipeImport.message;
                return 5;
            }
            metrics.recipeRows = recipeImport.data.storedRows;
            metrics.firstStartupAndImportMs = startupTimer.elapsed();

            UserProfile user;
            user.id = QStringLiteral("PERF_U001");
            user.name = QStringLiteral("性能测试用户");
            user.gender = Gender::Male;
            user.age = 25;
            user.heightCm = 175.0;
            user.weightKg = 80.0;
            user.targetWeightKg = 70.0;
            user.averageDailySteps = 7500;
            user.activityLevel = 1;
            user.goalType = GoalType::Lose;
            user.exerciseGoal = ExerciseGoal::LightHealth;
            user.includeSnack = false;
            user.weeklyGoalKg = 0.5;
            user.dietContributionRatio = 0.7;
            if (!users.add(user).ok) return 6;

            FeedbackService feedbackService(feedback);
            HealthCalculator healthCalculator;
            WeeklyPlanner weeklyPlanner;
            PlanGenerationService planService(
                users,
                exercises,
                recipes,
                plans,
                healthCalculator,
                weeklyPlanner,
                feedbackService);
            WeeklyPlanOptions options;
            options.randomSeed = static_cast<quint32>(20260909 + trial);
            options.dailyTargetVariationRatio = 0.0;

            QElapsedTimer planTimer;
            planTimer.start();
            const auto plan = planService.generateAndSave(
                user.id,
                QDate(2026, 9, 7).addDays(trial * 7),
                options);
            metrics.weeklyPlanMs = planTimer.elapsed();
            if (!plan.ok || plan.data.days.size() != 7) {
                qCritical().noquote() << "Plan generation failed:"
                                      << plan.code << plan.message;
                return 7;
            }
        }

        {
            QElapsedTimer unchangedTimer;
            unchangedTimer.start();

            DatabaseManager manager(databasePath);
            if (!openAndInitialize(manager)) return 8;
            SqliteExerciseRepository exercises(manager.database());
            SqliteRecipeRepository recipes(manager.database());
            CsvDataExchangeService exchange;
            BuiltinDatasetInitializer initializer(
                manager.database(), exercises, recipes, exchange);
            const auto exerciseCheck = initializer.importExercisesIfChanged(
                QStringLiteral("builtin_exercises"), exercisePath);
            const auto recipeCheck = initializer.importRecipesIfChanged(
                QStringLiteral("builtin_recipes"), recipePath);
            metrics.unchangedStartupMs = unchangedTimer.elapsed();
            if (!exerciseCheck.ok || !recipeCheck.ok
                || exerciseCheck.data.imported || recipeCheck.data.imported) {
                qCritical() << "Unchanged dataset check did not skip imports.";
                return 9;
            }
        }

        results.append(metrics);
        output
            << QStringLiteral(
                   "PERF trial=%1 first_startup_import_ms=%2 "
                   "unchanged_startup_ms=%3 weekly_plan_ms=%4 "
                   "large_recipe_import_ms=%5 recipe_rows=%6")
                   .arg(trial + 1)
                   .arg(metrics.firstStartupAndImportMs)
                   .arg(metrics.unchangedStartupMs)
                   .arg(metrics.weeklyPlanMs)
                   .arg(metrics.largeRecipeImportMs)
                   .arg(metrics.recipeRows)
            << Qt::endl;
    }

    output
        << QStringLiteral(
               "PERF average first_startup_import_ms=%1 "
               "unchanged_startup_ms=%2 weekly_plan_ms=%3 "
               "large_recipe_import_ms=%4")
               .arg(average(results, &TrialResult::firstStartupAndImportMs),
                    0, 'f', 1)
               .arg(average(results, &TrialResult::unchangedStartupMs),
                    0, 'f', 1)
               .arg(average(results, &TrialResult::weeklyPlanMs),
                    0, 'f', 1)
               .arg(average(results, &TrialResult::largeRecipeImportMs),
                    0, 'f', 1)
        << Qt::endl;
    return 0;
}
