#include "application/csvdataexchangeservice.h"
#include "database/databasemanager.h"
#include "repositories/sqliteexerciserepository.h"
#include "repositories/sqlitereciperepository.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

namespace {

bool writeUtf8File(const QString& path, const QByteArray& contents)
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

    const QString exercisePath = directory.filePath(
        QStringLiteral("exercises.csv"));
    if (!writeUtf8File(
            exercisePath,
            QStringLiteral(
                "name,mets,type,description\n"
                "\"快走, 平地\",4.3,有氧,中等速度\n"
                "无效运动,abc,有氧,错误数据\n")
                .toUtf8())) {
        return 2;
    }

    CsvDataExchangeService service;
    const auto exercises = service.importExercises(
        exercisePath, DataFormat::Csv);
    if (!exercises.ok
        || exercises.data.importedRows != 1
        || exercises.data.skippedRows != 1
        || exercises.data.items.size() != 1) {
        return 3;
    }
    const Exercise& exercise = exercises.data.items.first();
    if (exercise.name != QStringLiteral("快走, 平地")
        || exercise.metValue != 4.3
        || exercise.category != ExerciseCategory::Aerobic
        || !exercise.id.startsWith(QStringLiteral("EXD_"))) {
        return 4;
    }

    const QString officialCompendiumPath = directory.filePath(
        QStringLiteral("official_compendium_export.csv"));
    if (!writeUtf8File(
            officialCompendiumPath,
            QByteArray(
                "2024 Adult Compendium of Physical Activities,,,,,,,,,,,,\n"
                "Major Heading,,Activity Code,,,,,MET Value,,,,Activity Description,\n"
                "Bicycling,,01010,,,,,4.0,4.0,4.0,4.0,"
                "\"Bicycling, leisure\",\n"
                "Conditioning Exercise,,,,,02052,,,5.0,5.0,5.0,"
                "\"Resistance weight training, squats\",\n"))) {
        return 24;
    }
    const auto officialExercises = service.importExercises(
        officialCompendiumPath, DataFormat::Csv);
    if (!officialExercises.ok
        || officialExercises.data.importedRows != 2
        || officialExercises.data.items.size() != 2
        || officialExercises.data.items.at(0).id != QStringLiteral("01010")
        || officialExercises.data.items.at(0).metValue != 4.0
        || officialExercises.data.items.at(0).category
            != ExerciseCategory::Aerobic
        || officialExercises.data.items.at(1).id != QStringLiteral("02052")
        || officialExercises.data.items.at(1).metValue != 5.0
        || officialExercises.data.items.at(1).category
            != ExerciseCategory::Strength) {
        return 25;
    }

    const QString recipePath = directory.filePath(
        QStringLiteral("recipes.csv"));
    if (!writeUtf8File(
            recipePath,
            QStringLiteral(
                "title,kcal,ProteinContent,CarbohydrateContent,"
                "FatContent,SaturatedFatContent,FiberContent,"
                "SugarContent,SodiumContent,CholesterolContent,"
                "RecipeServings,meal,ingredients,tags\n"
                "鸡胸肉饭,620,45,70,18,4,8,5,560,85,2,"
                "午餐,鸡胸肉:150:g|米饭:150:g,高蛋白|低脂\n"
                "无效餐,500,0,0,0,0,0,0,0,0,1,宵夜,,\n")
                .toUtf8())) {
        return 5;
    }

    const auto recipes = service.importRecipes(recipePath, DataFormat::Csv);
    if (!recipes.ok
        || recipes.data.importedRows != 1
        || recipes.data.skippedRows != 1
        || recipes.data.items.size() != 1) {
        return 6;
    }
    const Recipe& recipe = recipes.data.items.first();
    if (recipe.mealType != MealType::Lunch
        || recipe.ingredients.size() != 2
        || recipe.ingredients.first().amount != 150.0
        || recipe.nutritionTags.size() != 2
        || recipe.totalCalories != 620.0
        || recipe.nutritionPerServing.caloriesKcal != 620.0
        || recipe.nutritionPerServing.proteinG != 45.0
        || recipe.nutritionPerServing.carbohydrateG != 70.0
        || recipe.nutritionPerServing.fatG != 18.0
        || recipe.nutritionPerServing.saturatedFatG != 4.0
        || recipe.nutritionPerServing.fiberG != 8.0
        || recipe.nutritionPerServing.sugarG != 5.0
        || recipe.nutritionPerServing.sodiumMg != 560.0
        || recipe.nutritionPerServing.cholesterolMg != 85.0
        || recipe.servings != 2
        || !recipe.id.startsWith(QStringLiteral("RD_"))) {
        return 7;
    }

    const QString foodComPath = directory.filePath(
        QStringLiteral("foodcom_recipes.csv"));
    if (!writeUtf8File(
            foodComPath,
            QStringLiteral(
                "RecipeId,Name,RecipeCategory,Keywords,"
                "RecipeIngredientQuantities,RecipeIngredientParts,"
                "Calories,FatContent,SaturatedFatContent,CholesterolContent,"
                "SodiumContent,CarbohydrateContent,FiberContent,SugarContent,"
                "ProteinContent,RecipeServings,Description\n"
                "1001,Protein Pancakes,Breakfast,"
                "\"c(\"\"breakfast\"\", \"\"high-protein\"\")\","
                "\"c(\"\"1 1/2 cups\"\", \"\"2\"\")\","
                "\"c(\"\"oats\"\", \"\"eggs\"\")\","
                "430,10,2,180,350,55,8,6,28,2,"
                "\"First description line\nsecond description line\"\n"
                "1002,Chicken Pasta,Chicken Breast,"
                "\"c(\"\"main-dish\"\", \"\"pasta\"\")\","
                "\"c(\"\"150 g\"\", \"\"1 cup\"\")\","
                "\"c(\"\"chicken breast\"\", \"\"pasta\"\")\","
                "520,14,3,75,480,62,7,5,36,2,Main dish\n"
                "1003,Mystery Dish,Miscellaneous,"
                "\"c(\"\"easy\"\")\",\"c(\"\"1\"\")\","
                "\"c(\"\"ingredient\"\")\",300,5,1,0,100,40,2,3,8,1,Unknown\n")
                .toUtf8())) {
        return 20;
    }

    const auto foodComRecipes = service.importRecipes(
        foodComPath, DataFormat::Csv);
    if (!foodComRecipes.ok
        || foodComRecipes.data.importedRows != 2
        || foodComRecipes.data.skippedRows != 1
        || foodComRecipes.data.items.size() != 2) {
        return 21;
    }

    const Recipe& foodComBreakfast = foodComRecipes.data.items.at(0);
    if (foodComBreakfast.id != QStringLiteral("1001")
        || foodComBreakfast.mealType != MealType::Breakfast
        || foodComBreakfast.ingredients.size() != 2
        || foodComBreakfast.ingredients.at(0).name != QStringLiteral("oats")
        || foodComBreakfast.ingredients.at(0).amount != 1.5
        || foodComBreakfast.ingredients.at(0).unit != QStringLiteral("cups")
        || foodComBreakfast.ingredients.at(1).amount != 2.0
        || !foodComBreakfast.nutritionTags.contains(
            QStringLiteral("high-protein"))
        || foodComBreakfast.nutritionPerServing.caloriesKcal != 430.0
        || foodComBreakfast.nutritionPerServing.proteinG != 28.0
        || foodComBreakfast.nutritionPerServing.carbohydrateG != 55.0
        || foodComBreakfast.nutritionPerServing.fatG != 10.0
        || foodComBreakfast.servings != 2) {
        return 22;
    }

    const Recipe& foodComDinner = foodComRecipes.data.items.at(1);
    if (foodComDinner.id != QStringLiteral("1002")
        || foodComDinner.mealType != MealType::Dinner
        || foodComDinner.ingredients.size() != 2
        || foodComDinner.ingredients.first().amount != 150.0
        || foodComDinner.ingredients.first().unit != QStringLiteral("g")) {
        return 23;
    }

    if (service.importExercises(exercisePath, DataFormat::Json).ok) return 8;

    DatabaseManager manager(directory.filePath(QStringLiteral("import.db")));
    QString databaseError;
    if (!manager.open(&databaseError) || !manager.initialize(&databaseError)) {
        return 9;
    }
    SqliteExerciseRepository exerciseRepository(manager.database());
    SqliteRecipeRepository recipeRepository(manager.database());
    if (!exerciseRepository.add(exercise).ok) return 10;
    if (!recipeRepository.add(recipe).ok) return 11;

    const auto storedExercises = exerciseRepository.findAll();
    const auto storedRecipes = recipeRepository.findAll();
    if (!storedExercises.ok
        || storedExercises.data.size() != 1
        || storedExercises.data.first().name != exercise.name) {
        return 12;
    }
    if (!storedRecipes.ok
        || storedRecipes.data.size() != 1
        || storedRecipes.data.first().ingredients.size() != 2
        || storedRecipes.data.first().nutritionPerServing.proteinG != 45.0
        || storedRecipes.data.first().nutritionPerServing.carbohydrateG != 70.0
        || storedRecipes.data.first().nutritionPerServing.fatG != 18.0
        || storedRecipes.data.first().servings != 2) {
        return 13;
    }
    return 0;
}
