#include "mainwindow.h"
#include "application/builtindatasetinitializer.h"
#include "application/plangenerationservice.h"
#include "application/csvdataexchangeservice.h"
#include "database/databasemanager.h"
#include "repositories/sqliteexerciserepository.h"
#include "repositories/sqliteplanrepository.h"
#include "repositories/sqlitereciperepository.h"
#include "repositories/sqliteuserrepository.h"
#include "recommendation/healthcalculator.h"
#include "recommendation/WeeklyPlanner.h"
#include "session/sessionmanager.h"
#include "ui/appstyle.h"

#include <QApplication>
#include <QMessageBox>

#include "recommendation/recommendationcore.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    a.setStyleSheet(applicationStyleSheet());
    QCoreApplication::setOrganizationName(QStringLiteral("SummerSchool"));
    QCoreApplication::setApplicationName(QStringLiteral("WeightLossingAgent"));

    DatabaseManager databaseManager;
    QString databaseError;
    if (!databaseManager.open(&databaseError)
        || !databaseManager.initialize(&databaseError)) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("Database error"),
            databaseError);
        return 1;
    }

    SqliteExerciseRepository exerciseRepository(databaseManager.database());
    SqliteRecipeRepository recipeRepository(databaseManager.database());
    SqlitePlanRepository planRepository(databaseManager.database());
    SqliteUserRepository userRepository(databaseManager.database());
    CsvDataExchangeService dataExchangeService;
    BuiltinDatasetInitializer datasetInitializer(
        databaseManager.database(),
        exerciseRepository,
        recipeRepository,
        dataExchangeService);
    const auto exerciseDatasetResult =
        datasetInitializer.importExercisesIfChanged(
            QStringLiteral("builtin_exercises"),
            QStringLiteral(":/datasets/exercises.csv"));
    if (!exerciseDatasetResult.ok) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("Dataset error"),
            exerciseDatasetResult.message);
        return 1;
    }
    const auto recipeDatasetResult = datasetInitializer.importRecipesIfChanged(
        QStringLiteral("builtin_recipes"),
        QStringLiteral(":/datasets/recipes.csv"));
    if (!recipeDatasetResult.ok) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("Dataset error"),
            recipeDatasetResult.message);
        return 1;
    }

    HealthCalculator healthCalculator;
    WeeklyPlanner weeklyPlanner;
    PlanGenerationService planGenerationService(userRepository,
                                                exerciseRepository,
                                                recipeRepository,
                                                planRepository,
                                                healthCalculator,
                                                weeklyPlanner);
    SessionManager sessionManager;
    MainWindow w(exerciseRepository,
                 recipeRepository,
                 planRepository,
                 planGenerationService,
                 dataExchangeService,
                 userRepository,
                 healthCalculator,
                 sessionManager);
    w.show();
    return QApplication::exec();
}
