#include "application/builtindatasetinitializer.h"
#include "application/compendiumpreprocessor.h"
#include "application/csvdataexchangeservice.h"
#include "application/foodcompreprocessor.h"
#include "database/databasemanager.h"
#include "repositories/sqliteexerciserepository.h"
#include "repositories/sqlitereciperepository.h"

#include <QCoreApplication>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>

#include <cmath>

namespace {

bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(contents) == contents.size();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;

    const QString rawPath = directory.filePath(QStringLiteral("foodcom.csv"));
    const QByteArray rawCsv = QByteArray(
        R"CSV(RecipeId,Name,RecipeCategory,Keywords,RecipeIngredientQuantities,RecipeIngredientParts,Calories,FatContent,SaturatedFatContent,CholesterolContent,SodiumContent,CarbohydrateContent,FiberContent,SugarContent,ProteinContent,RecipeServings
B1,Protein Oats,Breakfast,"c(""breakfast"", ""high-protein"")","c(""1 cup"", ""2"")","c(""oats"", ""eggs"")",400,10,2,120,300,48,8,4,30,1
B2,Fruit Oats,Breakfast,"c(""breakfast"", ""fruit"")","c(""1 cup"", ""1 cup"")","c(""oats"", ""berries"")",390,8,1,0,120,62,9,18,14,1
L1,Chicken Rice,Lunch,"c(""lunch"", ""high-protein"")","c(""150 g"", ""1 cup"")","c(""chicken"", ""rice"")",600,16,4,80,480,70,6,4,45,1
D1,Fish Pasta,Main Dish,"c(""main-dish"", ""seafood"")","c(""180 g"", ""1 cup"")","c(""fish"", ""pasta"")",580,18,4,70,420,64,7,5,42,1
S1,Yogurt Berries,Snack,"c(""snack"", ""high-protein"")","c(""200 g"", ""80 g"")","c(""yogurt"", ""berries"")",250,7,2,15,110,32,5,16,16,1
)CSV");
    if (!writeFile(rawPath, rawCsv)) return 2;

    CsvDataExchangeService dataExchangeService;
    FoodComPreprocessor preprocessor(dataExchangeService);
    FoodComPreprocessOptions options;
    options.maximumRecipesPerMeal = 1;
    const QString outputPath = directory.filePath(
        QStringLiteral("builtin_recipes.csv"));
    const auto firstPreprocess = preprocessor.preprocess(
        rawPath, outputPath, options);
    if (!firstPreprocess.ok || firstPreprocess.data.selectedRows != 4) return 3;

    const auto normalizedRecipes = dataExchangeService.importRecipes(
        outputPath, DataFormat::Csv);
    if (!normalizedRecipes.ok
        || normalizedRecipes.data.items.size() != 4) {
        return 4;
    }

    DatabaseManager manager(
        directory.filePath(QStringLiteral("dataset_pipeline.db")));
    QString databaseError;
    if (!manager.open(&databaseError) || !manager.initialize(&databaseError)) {
        return 5;
    }

    SqliteRecipeRepository recipeRepository(manager.database());
    SqliteExerciseRepository exerciseRepository(manager.database());
    BuiltinDatasetInitializer initializer(
        manager.database(),
        exerciseRepository,
        recipeRepository,
        dataExchangeService);
    const auto firstImport = initializer.importRecipesIfChanged(
        QStringLiteral("test_recipes"), outputPath);
    if (!firstImport.ok
        || !firstImport.data.imported
        || firstImport.data.storedRows != 4) {
        return 6;
    }

    const auto unchangedImport = initializer.importRecipesIfChanged(
        QStringLiteral("test_recipes"), outputPath);
    if (!unchangedImport.ok || unchangedImport.data.imported) return 7;

    options.maximumRecipesPerMeal = 2;
    const auto secondPreprocess = preprocessor.preprocess(
        rawPath, outputPath, options);
    if (!secondPreprocess.ok || secondPreprocess.data.selectedRows != 5) return 8;

    const auto changedImport = initializer.importRecipesIfChanged(
        QStringLiteral("test_recipes"), outputPath);
    if (!changedImport.ok
        || !changedImport.data.imported
        || changedImport.data.storedRows != 5) {
        return 9;
    }

    const auto storedRecipes = recipeRepository.findAll();
    if (!storedRecipes.ok || storedRecipes.data.size() != 5) return 10;

    const auto resourceImport = initializer.importRecipesIfChanged(
        QStringLiteral("resource_recipes"),
        QStringLiteral(":/datasets/recipes.csv"));
    if (!resourceImport.ok
        || !resourceImport.data.imported
        || resourceImport.data.storedRows != 1520) {
        return 11;
    }

    const auto unchangedResourceImport = initializer.importRecipesIfChanged(
        QStringLiteral("resource_recipes"),
        QStringLiteral(":/datasets/recipes.csv"));
    if (!unchangedResourceImport.ok
        || unchangedResourceImport.data.imported) {
        return 12;
    }

    const QString rawExercisePath = directory.filePath(
        QStringLiteral("compendium.csv"));
    const QByteArray rawExerciseCsv = QByteArray(
        R"CSV(Major Heading,Activity Code,MET Value,Activity Description
Bicycling,01010,4.0,"Bicycling, leisure"
Running,12020,8.0,"Running, moderate pace"
Conditioning Exercises,02050,5.0,"Resistance training, moderate effort"
Conditioning Exercises,02150,2.3,"Yoga, Hatha"
Conditioning Exercises,02190,3.0,"Balance exercise, general"
Inactivity,07010,1.3,"Sitting quietly"
Running,12030,20.0,"Running, extreme effort"
)CSV");
    if (!writeFile(rawExercisePath, rawExerciseCsv)) return 13;

    CompendiumPreprocessor compendiumPreprocessor(dataExchangeService);
    CompendiumPreprocessOptions exerciseOptions;
    exerciseOptions.maximumExercisesPerCategory = 1;
    const QString exerciseOutputPath = directory.filePath(
        QStringLiteral("builtin_exercises.csv"));
    const auto exercisePreprocess = compendiumPreprocessor.preprocess(
        rawExercisePath, exerciseOutputPath, exerciseOptions);
    if (!exercisePreprocess.ok
        || exercisePreprocess.data.selectedRows != 4) {
        return 14;
    }

    const auto normalizedExercises = dataExchangeService.importExercises(
        exerciseOutputPath, DataFormat::Csv);
    if (!normalizedExercises.ok
        || normalizedExercises.data.items.size() != 4) {
        return 15;
    }
    QSet<ExerciseCategory> normalizedCategories;
    for (const Exercise& exercise : normalizedExercises.data.items) {
        normalizedCategories.insert(exercise.category);
    }
    if (!normalizedCategories.contains(ExerciseCategory::Aerobic)
        || !normalizedCategories.contains(ExerciseCategory::Strength)
        || !normalizedCategories.contains(ExerciseCategory::Flexibility)
        || !normalizedCategories.contains(ExerciseCategory::Balance)) {
        return 20;
    }

    const auto firstExerciseImport = initializer.importExercisesIfChanged(
        QStringLiteral("test_exercises"), exerciseOutputPath);
    if (!firstExerciseImport.ok
        || !firstExerciseImport.data.imported
        || firstExerciseImport.data.storedRows != 4) {
        return 16;
    }
    const auto unchangedExerciseImport =
        initializer.importExercisesIfChanged(
            QStringLiteral("test_exercises"), exerciseOutputPath);
    if (!unchangedExerciseImport.ok
        || unchangedExerciseImport.data.imported) {
        return 17;
    }

    const auto exerciseResourceImport = initializer.importExercisesIfChanged(
        QStringLiteral("resource_exercises"),
        QStringLiteral(":/datasets/exercises.csv"));
    if (!exerciseResourceImport.ok
        || !exerciseResourceImport.data.imported
        || exerciseResourceImport.data.storedRows != 178) {
        return 18;
    }
    const auto unchangedExerciseResourceImport =
        initializer.importExercisesIfChanged(
            QStringLiteral("resource_exercises"),
            QStringLiteral(":/datasets/exercises.csv"));
    if (!unchangedExerciseResourceImport.ok
        || unchangedExerciseResourceImport.data.imported) {
        return 19;
    }

    QByteArray largeRecipeCsv = QByteArrayLiteral(
        "id,name,total_calories,meal_type,ingredients,protein_g,"
        "carbohydrate_g,fat_g,fiber_g,sodium_mg\n");
    const QStringList mealTypes = {
        QStringLiteral("breakfast"),
        QStringLiteral("lunch"),
        QStringLiteral("dinner"),
        QStringLiteral("snack")
    };
    const QVector<int> mealTargets = {450, 600, 600, 250};
    for (qsizetype mealIndex = 0;
         mealIndex < mealTypes.size();
         ++mealIndex) {
        for (int itemIndex = 0; itemIndex < 100; ++itemIndex) {
            const int calories = mealTargets.at(mealIndex)
                + (itemIndex - 50) * 2;
            largeRecipeCsv.append(
                QStringLiteral(
                    "LARGE_R_%1_%2,Recipe %1 %2,%3,%4,"
                    "ingredient:1:g|vegetable:100:g,%5,60,15,%6,%7\n")
                    .arg(mealIndex)
                    .arg(itemIndex, 3, 10, QLatin1Char('0'))
                    .arg(calories)
                    .arg(mealTypes.at(mealIndex))
                    .arg(20 + itemIndex % 20)
                    .arg(5 + itemIndex % 8)
                    .arg(200 + itemIndex % 200)
                    .toUtf8());
        }
    }
    const QString largeRecipePath = directory.filePath(
        QStringLiteral("large_recipes.csv"));
    if (!writeFile(largeRecipePath, largeRecipeCsv)) return 21;
    const auto largeRecipeImport = initializer.importRecipesIfChanged(
        QStringLiteral("large_recipes"), largeRecipePath);
    if (!largeRecipeImport.ok
        || largeRecipeImport.data.storedRows != 400) {
        return 22;
    }

    QByteArray largeExerciseCsv = QByteArrayLiteral(
        "id,name,met_value,category,description\n");
    const QStringList exerciseCategories = {
        QStringLiteral("aerobic"),
        QStringLiteral("strength"),
        QStringLiteral("flexibility"),
        QStringLiteral("balance")
    };
    for (qsizetype categoryIndex = 0;
         categoryIndex < exerciseCategories.size();
         ++categoryIndex) {
        for (int itemIndex = 0; itemIndex < 60; ++itemIndex) {
            largeExerciseCsv.append(
                QStringLiteral(
                    "LARGE_E_%1_%2,Exercise %1 %2,%3,%4,Generated test item\n")
                    .arg(categoryIndex)
                    .arg(itemIndex, 3, 10, QLatin1Char('0'))
                    .arg(2.0 + itemIndex * 0.2, 0, 'f', 1)
                    .arg(exerciseCategories.at(categoryIndex))
                    .toUtf8());
        }
    }
    const QString largeExercisePath = directory.filePath(
        QStringLiteral("large_exercises.csv"));
    if (!writeFile(largeExercisePath, largeExerciseCsv)) return 23;
    const auto largeExerciseImport = initializer.importExercisesIfChanged(
        QStringLiteral("large_exercises"), largeExercisePath);
    if (!largeExerciseImport.ok
        || largeExerciseImport.data.storedRows != 240) {
        return 24;
    }

    RecipeFilter recipeSearch;
    recipeSearch.mealType = MealType::Lunch;
    recipeSearch.minimumProteinG = 20.0;
    recipeSearch.maximumSodiumMg = 500.0;
    recipeSearch.targetCalories = 600.0;
    recipeSearch.limit = 12;
    const auto recipeCandidates = recipeRepository.findAll(recipeSearch);
    if (!recipeCandidates.ok || recipeCandidates.data.size() != 12) return 25;
    double previousCalorieDistance = -1.0;
    for (const Recipe& recipe : recipeCandidates.data) {
        const double distance = std::abs(recipe.totalCalories - 600.0);
        if (distance < previousCalorieDistance) return 26;
        previousCalorieDistance = distance;
    }

    ExerciseFilter exerciseSearch;
    exerciseSearch.minimumMet = 2.0;
    exerciseSearch.maximumMet = 18.0;
    exerciseSearch.targetMet = 6.0;
    exerciseSearch.limit = 24;
    const auto exerciseCandidates = exerciseRepository.findAll(exerciseSearch);
    if (!exerciseCandidates.ok || exerciseCandidates.data.size() != 24) {
        return 27;
    }
    double previousMetDistance = -1.0;
    for (const Exercise& exercise : exerciseCandidates.data) {
        const double distance = std::abs(exercise.metValue - 6.0);
        if (distance < previousMetDistance) return 28;
        previousMetDistance = distance;
    }
    return 0;
}
