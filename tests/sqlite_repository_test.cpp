#include "database/databasemanager.h"
#include "repositories/sqliteexerciserepository.h"
#include "repositories/sqlitefeedbackrepository.h"
#include "repositories/sqliteplanrepository.h"
#include "repositories/sqlitereciperepository.h"
#include "repositories/sqliteuserrepository.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <cmath>
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

    // Simulate a database created by schema version 4. initialize() must add
    // average_daily_steps without requiring the user to edit the DB file.
    QSqlQuery legacySchemaQuery(manager.database());
    if (!legacySchemaQuery.exec(QStringLiteral(R"SQL(
        CREATE TABLE users (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            gender TEXT NOT NULL CHECK(gender IN ('M', 'F')),
            age INTEGER NOT NULL CHECK(age > 0),
            height_cm REAL NOT NULL CHECK(height_cm > 0),
            weight_kg REAL NOT NULL CHECK(weight_kg > 0),
            target_weight_kg REAL NOT NULL CHECK(target_weight_kg > 0),
            activity_level INTEGER NOT NULL
            CHECK(activity_level BETWEEN 1 AND 5),
            goal_type TEXT NOT NULL
            CHECK(goal_type IN ('lose', 'maintain', 'gain')),
            weekly_goal_kg REAL NOT NULL DEFAULT 0.5,
            diet_contribution_ratio REAL NOT NULL DEFAULT 0.7,
            disliked_exercise_ids_json TEXT NOT NULL DEFAULT '[]',
            disliked_recipe_ids_json TEXT NOT NULL DEFAULT '[]'
        )
    )SQL"))) return 32;
    if (!manager.initialize(&error)) return 3;
    if (!manager.seedDemoData(&error)) return 4;

    QSqlQuery versionQuery(manager.database());
    if (!versionQuery.exec(QStringLiteral("PRAGMA user_version"))
        || !versionQuery.next()
        || versionQuery.value(0).toInt() != 5) {
        return 26;
    }

    SqliteUserRepository userRepository(manager.database());
    SqliteExerciseRepository exerciseRepository(manager.database());
    SqliteRecipeRepository recipeRepository(manager.database());
    SqlitePlanRepository planRepository(manager.database());
    SqliteFeedbackRepository feedbackRepository(manager.database());

    const auto users = userRepository.findAll();
    if (!users.ok || users.data.size() != 1
        || users.data.first().averageDailySteps != 4000) return 5;

    UserProfile updatedUser = users.data.first();
    updatedUser.averageDailySteps = 8250;
    if (!userRepository.update(updatedUser).ok) return 30;
    const auto reloadedUser = userRepository.findById(updatedUser.id);
    if (!reloadedUser.ok || !reloadedUser.data.has_value()
        || reloadedUser.data->averageDailySteps != 8250) return 31;

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
    if (!recipes.ok || recipes.data.size() != 6) return 11;

    Recipe nutritionRecipe;
    nutritionRecipe.id = QStringLiteral("R_NUTRITION_TEST");
    nutritionRecipe.name = QStringLiteral("Nutrition Repository Test");
    nutritionRecipe.ingredients = {
        {QStringLiteral("Chicken breast"), 150.0, QStringLiteral("g")},
        {QStringLiteral("Brown rice"), 120.0, QStringLiteral("g")}
    };
    nutritionRecipe.totalCalories = 510.0;
    nutritionRecipe.nutritionPerServing.caloriesKcal = 510.0;
    nutritionRecipe.nutritionPerServing.proteinG = 35.5;
    nutritionRecipe.nutritionPerServing.carbohydrateG = 48.0;
    nutritionRecipe.nutritionPerServing.fatG = 12.5;
    nutritionRecipe.nutritionPerServing.saturatedFatG = 3.0;
    nutritionRecipe.nutritionPerServing.fiberG = 6.5;
    nutritionRecipe.nutritionPerServing.sugarG = 4.0;
    nutritionRecipe.nutritionPerServing.sodiumMg = 420.0;
    nutritionRecipe.nutritionPerServing.cholesterolMg = 72.0;
    nutritionRecipe.servings = 2;
    nutritionRecipe.mealType = MealType::Lunch;
    nutritionRecipe.nutritionTags = {
        QStringLiteral("high-protein"),
        QStringLiteral("high-fiber")
    };

    if (!recipeRepository.add(nutritionRecipe).ok) return 20;

    const auto loadedNutritionRecipe =
        recipeRepository.findById(nutritionRecipe.id);
    if (!loadedNutritionRecipe.ok
        || !loadedNutritionRecipe.data.has_value()) {
        return 21;
    }

    const auto almostEqual = [](double left, double right) {
        return std::abs(left - right) < 0.000001;
    };
    const Recipe& storedRecipe = *loadedNutritionRecipe.data;
    const NutritionFacts& storedNutrition =
        storedRecipe.nutritionPerServing;

    if (storedRecipe.servings != nutritionRecipe.servings
        || !almostEqual(storedRecipe.totalCalories, 510.0)
        || !almostEqual(storedNutrition.caloriesKcal, 510.0)
        || !almostEqual(storedNutrition.proteinG, 35.5)
        || !almostEqual(storedNutrition.carbohydrateG, 48.0)
        || !almostEqual(storedNutrition.fatG, 12.5)
        || !almostEqual(storedNutrition.saturatedFatG, 3.0)
        || !almostEqual(storedNutrition.fiberG, 6.5)
        || !almostEqual(storedNutrition.sugarG, 4.0)
        || !almostEqual(storedNutrition.sodiumMg, 420.0)
        || !almostEqual(storedNutrition.cholesterolMg, 72.0)) {
        return 22;
    }

    nutritionRecipe.nutritionPerServing.proteinG = 40.0;
    nutritionRecipe.servings = 3;
    if (!recipeRepository.update(nutritionRecipe).ok) return 23;

    const auto updatedNutritionRecipe =
        recipeRepository.findById(nutritionRecipe.id);
    if (!updatedNutritionRecipe.ok
        || !updatedNutritionRecipe.data.has_value()
        || updatedNutritionRecipe.data->servings != 3
        || !almostEqual(
            updatedNutritionRecipe.data->nutritionPerServing.proteinG,
            40.0)) {
        return 24;
    }

    RecipeFilter candidateFilter;
    candidateFilter.mealType = MealType::Lunch;
    candidateFilter.minimumProteinG = 35.0;
    candidateFilter.minimumFiberG = 5.0;
    candidateFilter.maximumSodiumMg = 500.0;
    candidateFilter.targetCalories = 500.0;
    candidateFilter.limit = 1;
    const auto candidateRecipes = recipeRepository.findAll(candidateFilter);
    if (!candidateRecipes.ok
        || candidateRecipes.data.size() != 1
        || candidateRecipes.data.first().id != nutritionRecipe.id) {
        return 27;
    }

    candidateFilter.excludedIds = {nutritionRecipe.id};
    const auto excludedCandidateRecipes =
        recipeRepository.findAll(candidateFilter);
    if (!excludedCandidateRecipes.ok
        || !excludedCandidateRecipes.data.isEmpty()) {
        return 28;
    }

    ExerciseFilter exerciseCandidateFilter;
    exerciseCandidateFilter.targetMet = 6.0;
    exerciseCandidateFilter.limit = 1;
    const auto exerciseCandidates =
        exerciseRepository.findAll(exerciseCandidateFilter);
    if (!exerciseCandidates.ok
        || exerciseCandidates.data.size() != 1
        || exerciseCandidates.data.first().id != QStringLiteral("EX002")) {
        return 29;
    }

    if (!recipeRepository.remove(nutritionRecipe.id).data) return 25;

    WeeklyPlan plan;
    plan.planId = QStringLiteral("PLAN_TEST");
    plan.userId = QStringLiteral("U001");
    plan.startDate = QDate(2026, 8, 26);
    plan.generatedAt = QDateTime::currentDateTimeUtc();
    plan.days.append(DailyPlan{plan.startDate});
    plan.days.first().meals.totalNutrition.caloriesKcal = 510.0;
    plan.days.first().meals.totalNutrition.proteinG = 35.5;
    plan.days.first().meals.breakfast.append(MealPlanItem{});
    plan.days.first().meals.breakfast.first().nutrition =
        nutritionRecipe.nutritionPerServing;
    if (!planRepository.save(plan).ok) return 12;

    const auto loadedPlan = planRepository.findById(plan.planId);
    if (!loadedPlan.ok || !loadedPlan.data.has_value()
        || loadedPlan.data->days.size() != 1
        || !almostEqual(
            loadedPlan.data->days.first().meals.totalNutrition.proteinG,
            35.5)
        || !almostEqual(
            loadedPlan.data->days.first().meals.breakfast.first()
                .nutrition.proteinG,
            40.0)) return 13;

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
