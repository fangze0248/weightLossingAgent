#include "sqlitereciperepository.h"

#include "database/modeljsoncodec.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace {

Recipe recipeFromQuery(const QSqlQuery& query)
{
    Recipe recipe;
    recipe.id = query.value(QStringLiteral("id")).toString();
    recipe.name = query.value(QStringLiteral("name")).toString();
    recipe.ingredients = model_json_codec::ingredientsFromJson(
        query.value(QStringLiteral("ingredients_json")).toString());
    recipe.totalCalories = query.value(QStringLiteral("total_calories")).toDouble();
    recipe.nutritionPerServing =
        model_json_codec::nutritionFactsFromJson(
            query.value(QStringLiteral("nutrition_json")).toString());

    const int storedServings =
        query.value(QStringLiteral("servings")).toInt();

    recipe.servings = storedServings > 0 ? storedServings : 1;

    // 兼容旧食谱：旧数据只有 total_calories
    if (recipe.nutritionPerServing.caloriesKcal <= 0.0) {
        recipe.nutritionPerServing.caloriesKcal =
            recipe.totalCalories;
    }
    recipe.mealType = mealTypeFromStorageString(
                          query.value(QStringLiteral("meal_type")).toString())
                          .value_or(MealType::Breakfast);
    recipe.nutritionTags = model_json_codec::stringListFromJson(
        query.value(QStringLiteral("nutrition_tags_json")).toString());
    return recipe;
}

bool containsAllTags(const Recipe& recipe, const QStringList& requiredTags)
{
    for (const QString& required : requiredTags) {
        bool found = false;
        for (const QString& actual : recipe.nutritionTags) {
            if (actual.compare(required, Qt::CaseInsensitive) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

ServiceResult<Recipe> validateRecipe(const Recipe& recipe)
{
    if (recipe.id.trimmed().isEmpty() || recipe.name.trimmed().isEmpty()) {
        return ServiceResult<Recipe>::failure(
            QStringLiteral("INVALID_RECIPE"),
            QStringLiteral("Recipe id and name are required."));
    }
    if (recipe.totalCalories < 0.0) {
        return ServiceResult<Recipe>::failure(
            QStringLiteral("INVALID_RECIPE"),
            QStringLiteral("Recipe calories cannot be negative."));
    }
    return ServiceResult<Recipe>::success(recipe);
}

void bindRecipe(QSqlQuery& query, const Recipe& recipe)
{
    NutritionFacts nutrition = recipe.nutritionPerServing;

    // Keep the legacy calorie column synchronized with the new nutrition data.
    if (nutrition.caloriesKcal <= 0.0) {
        nutrition.caloriesKcal = recipe.totalCalories;
    }

    query.bindValue(QStringLiteral(":id"), recipe.id.trimmed());
    query.bindValue(QStringLiteral(":name"), recipe.name.trimmed());
    query.bindValue(QStringLiteral(":ingredients"),
                    model_json_codec::ingredientsToJson(recipe.ingredients));
    query.bindValue(QStringLiteral(":calories"), nutrition.caloriesKcal);
    query.bindValue(QStringLiteral(":nutrition"),
                    model_json_codec::nutritionFactsToJson(nutrition));
    query.bindValue(QStringLiteral(":servings"), recipe.servings);
    query.bindValue(QStringLiteral(":meal_type"), toStorageString(recipe.mealType));
    query.bindValue(QStringLiteral(":tags"),
                    model_json_codec::stringListToJson(recipe.nutritionTags));
}

} // namespace

SqliteRecipeRepository::SqliteRecipeRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

ServiceResult<QVector<Recipe>> SqliteRecipeRepository::findAll(
    const RecipeFilter& filter) const
{
    QString sql = QStringLiteral("SELECT * FROM recipes WHERE 1 = 1");
    if (!filter.keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(" AND name LIKE :keyword");
    }
    if (filter.mealType.has_value()) {
        sql += QStringLiteral(" AND meal_type = :meal_type");
    }
    if (filter.minimumCalories.has_value()) {
        sql += QStringLiteral(" AND total_calories >= :minimum_calories");
    }
    if (filter.maximumCalories.has_value()) {
        sql += QStringLiteral(" AND total_calories <= :maximum_calories");
    }
    sql += QStringLiteral(" ORDER BY meal_type, name");

    QSqlQuery query(database_);
    query.prepare(sql);
    if (!filter.keyword.trimmed().isEmpty()) {
        query.bindValue(QStringLiteral(":keyword"),
                        QStringLiteral("%") + filter.keyword.trimmed()
                            + QStringLiteral("%"));
    }
    if (filter.mealType.has_value()) {
        query.bindValue(QStringLiteral(":meal_type"),
                        toStorageString(*filter.mealType));
    }
    if (filter.minimumCalories.has_value()) {
        query.bindValue(QStringLiteral(":minimum_calories"), *filter.minimumCalories);
    }
    if (filter.maximumCalories.has_value()) {
        query.bindValue(QStringLiteral(":maximum_calories"), *filter.maximumCalories);
    }

    if (!query.exec()) {
        return ServiceResult<QVector<Recipe>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }

    QVector<Recipe> recipes;
    while (query.next()) {
        Recipe recipe = recipeFromQuery(query);
        if (containsAllTags(recipe, filter.requiredNutritionTags)) {
            recipes.append(recipe);
        }
    }
    return ServiceResult<QVector<Recipe>>::success(recipes);
}

ServiceResult<std::optional<Recipe>> SqliteRecipeRepository::findById(
    const QString& id) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM recipes WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        return ServiceResult<std::optional<Recipe>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }
    if (!query.next()) {
        return ServiceResult<std::optional<Recipe>>::success(std::nullopt);
    }
    return ServiceResult<std::optional<Recipe>>::success(recipeFromQuery(query));
}

ServiceResult<Recipe> SqliteRecipeRepository::add(const Recipe& recipe)
{
    const auto validation = validateRecipe(recipe);
    if (!validation.ok) return validation;

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO recipes ("
        "id, name, ingredients_json, total_calories, "
        "nutrition_json, servings, meal_type, nutrition_tags_json"
        ") VALUES ("
        ":id, :name, :ingredients, :calories, "
        ":nutrition, :servings, :meal_type, :tags"
        ")"));
    bindRecipe(query, recipe);
    if (!query.exec()) {
        return ServiceResult<Recipe>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<Recipe>::success(recipe);
}

ServiceResult<Recipe> SqliteRecipeRepository::update(const Recipe& recipe)
{
    const auto validation = validateRecipe(recipe);
    if (!validation.ok) return validation;

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE recipes SET "
        "name = :name, "
        "ingredients_json = :ingredients, "
        "total_calories = :calories, "
        "nutrition_json = :nutrition, "
        "servings = :servings, "
        "meal_type = :meal_type, "
        "nutrition_tags_json = :tags "
        "WHERE id = :id"));
    bindRecipe(query, recipe);
    if (!query.exec()) {
        return ServiceResult<Recipe>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        return ServiceResult<Recipe>::failure(
            QStringLiteral("RECIPE_NOT_FOUND"), QStringLiteral("Recipe not found."));
    }
    return ServiceResult<Recipe>::success(recipe);
}

ServiceResult<bool> SqliteRecipeRepository::remove(const QString& id)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM recipes WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        return ServiceResult<bool>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<bool>::success(query.numRowsAffected() > 0);
}
