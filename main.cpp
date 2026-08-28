#include "mainwindow.h"
#include "application/plangenerationservice.h"
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
        || !databaseManager.initialize(&databaseError)
        || !databaseManager.seedDemoData(&databaseError)) {
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
                 userRepository,
                 healthCalculator,
                 sessionManager);
    w.show();
    return QApplication::exec();
}
