#include "application/plangenerationservice.h"
#include "database/databasemanager.h"
#include "recommendation/WeeklyPlanner.h"
#include "recommendation/healthcalculator.h"
#include "repositories/sqliteexerciserepository.h"
#include "repositories/sqliteplanrepository.h"
#include "repositories/sqlitereciperepository.h"
#include "repositories/sqliteuserrepository.h"

#include <QCoreApplication>
#include <QDate>
#include <QTemporaryDir>

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
    HealthCalculator healthCalculator;
    WeeklyPlanner weeklyPlanner;
    PlanGenerationService service(users,
                                  exercises,
                                  recipes,
                                  plans,
                                  healthCalculator,
                                  weeklyPlanner);

    const auto result = service.generateAndSave(
        QStringLiteral("U001"), QDate(2026, 8, 24));
    if (!result.ok || result.data.days.size() != 7) return 3;

    const auto savedPlans = plans.findByUserId(QStringLiteral("U001"));
    if (!savedPlans.ok || savedPlans.data.size() != 1) return 4;
    if (savedPlans.data.first().planId != result.data.planId) return 5;
    return 0;
}

