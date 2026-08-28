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

    const QString recipePath = directory.filePath(
        QStringLiteral("recipes.csv"));
    if (!writeUtf8File(
            recipePath,
            QStringLiteral(
                "title,kcal,meal,ingredients,tags\n"
                "鸡胸肉饭,620,午餐,鸡胸肉:150:g|米饭:150:g,高蛋白|低脂\n"
                "无效餐,500,宵夜,,\n")
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
        || !recipe.id.startsWith(QStringLiteral("RD_"))) {
        return 7;
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
        || storedRecipes.data.first().ingredients.size() != 2) {
        return 13;
    }
    return 0;
}
