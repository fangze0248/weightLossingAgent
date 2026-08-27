#include "database/databasemanager.h"
#include "repositories/sqliteexerciseRepository.h"
#include "repositories/sqlitefeedbackrepository.h"
#include "repositories/sqliteplanrepository.h"
#include "repositories/sqlitereciperepository.h"
#include "repositories/sqliteuserrepository.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QTemporaryDir>
#include <cstdio>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;

    DatabaseManager manager(
        temporaryDirectory.filePath(QStringLiteral("repository_test.db")));
    QString error;
    if (!manager.open(&error)) return 2;
    if (!manager.initialize(&error)) return 3;
    if (!manager.seedDemoData(&error)) return 4;

    SqliteUserRepository userRepository(manager.database());
    SqliteExerciseRepository exerciseRepository(manager.database());
    SqliteRecipeRepository recipeRepository(manager.database());
    SqlitePlanRepository planRepository(manager.database());
    SqliteFeedbackRepository feedbackRepository(manager.database());

    const auto users = userRepository.findAll();
    if (!users.ok || users.data.size() != 1) return 5;

    const auto seededExercises = exerciseRepository.findAll();
    if (!seededExercises.ok || seededExercises.data.size() != 3) return 6;

    Exercise testExercise;
    testExercise.id = QStringLiteral("EX_TEST");
    testExercise.name = QStringLiteral("Repository Test Exercise");
    testExercise.metValue = 5.5;
    testExercise.category = ExerciseCategory::Strength;
    testExercise.description = QStringLiteral("Temporary CRUD test item");
    if (!exerciseRepository.add(testExercise).ok) return 7;

    const auto foundExercise = exerciseRepository.findById(testExercise.id);
    if (!foundExercise.ok || !foundExercise.data.has_value()) return 8;

    testExercise.metValue = 6.0;
    if (!exerciseRepository.update(testExercise).ok) return 9;
    if (!exerciseRepository.remove(testExercise.id).data) return 10;

    const auto recipes = recipeRepository.findAll();
    if (!recipes.ok || recipes.data.size() != 3) return 11;

    WeeklyPlan plan;
    plan.planId = QStringLiteral("PLAN_TEST");
    plan.userId = QStringLiteral("U001");
    plan.startDate = QDate(2026, 8, 26);
    plan.generatedAt = QDateTime::currentDateTimeUtc();
    plan.days.append(DailyPlan{plan.startDate});
    if (!planRepository.save(plan).ok) return 12;

    const auto loadedPlan = planRepository.findById(plan.planId);
    if (!loadedPlan.ok || !loadedPlan.data.has_value()
        || loadedPlan.data->days.size() != 1) return 13;

    Feedback feedback;
    feedback.id = QStringLiteral("FB_TEST");
    feedback.userId = QStringLiteral("U001");
    feedback.itemType = RecommendationItemType::Exercise;
    feedback.itemId = QStringLiteral("EX001");
    feedback.rating = FeedbackRating::Like;
    const auto savedFeedback = feedbackRepository.save(feedback);
    if (!savedFeedback.ok) {
        qCritical().noquote() << savedFeedback.code << savedFeedback.message;
        const QByteArray diagnostic =
            (savedFeedback.code + QStringLiteral(": ") + savedFeedback.message).toUtf8();
        std::fprintf(stderr, "%s\n", diagnostic.constData());
        return 14;
    }

    const auto feedbackItems = feedbackRepository.findByUserId(
        QStringLiteral("U001"));
    if (!feedbackItems.ok || feedbackItems.data.size() != 1) return 15;

    if (!planRepository.remove(plan.planId).data) return 16;
    return 0;
}
