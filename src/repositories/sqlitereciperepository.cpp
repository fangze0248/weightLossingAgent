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
    query.bindValue(QStringLiteral(":protein"), nutrition.proteinG);
    query.bindValue(
        QStringLiteral(":carbohydrate"), nutrition.carbohydrateG);
    query.bindValue(QStringLiteral(":fat"), nutrition.fatG);
    query.bindValue(
        QStringLiteral(":saturated_fat"), nutrition.saturatedFatG);
    query.bindValue(QStringLiteral(":fiber"), nutrition.fiberG);
    query.bindValue(QStringLiteral(":sugar"), nutrition.sugarG);
    query.bindValue(QStringLiteral(":sodium"), nutrition.sodiumMg);
    query.bindValue(
        QStringLiteral(":cholesterol"), nutrition.cholesterolMg);
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
    if (filter.limit < 0) {
        return ServiceResult<QVector<Recipe>>::failure(
            QStringLiteral("INVALID_RECIPE_FILTER"),
            QStringLiteral("查询数量限制不能为负数。"));
    }

    QString sql = QStringLiteral("SELECT * FROM recipes WHERE 1 = 1");
    if (!filter.keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(
            " AND (name LIKE :keyword OR ingredients_json LIKE :keyword)");
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
    if (filter.minimumProteinG.has_value()) {
        sql += QStringLiteral(" AND protein_g >= :minimum_protein");
    }
    if (filter.maximumProteinG.has_value()) {
        sql += QStringLiteral(" AND protein_g <= :maximum_protein");
    }
    if (filter.minimumCarbohydrateG.has_value()) {
        sql += QStringLiteral(
            " AND carbohydrate_g >= :minimum_carbohydrate");
    }
    if (filter.maximumCarbohydrateG.has_value()) {
        sql += QStringLiteral(
            " AND carbohydrate_g <= :maximum_carbohydrate");
    }
    if (filter.minimumFatG.has_value()) {
        sql += QStringLiteral(" AND fat_g >= :minimum_fat");
    }
    if (filter.maximumFatG.has_value()) {
        sql += QStringLiteral(" AND fat_g <= :maximum_fat");
    }
    if (filter.minimumFiberG.has_value()) {
        sql += QStringLiteral(" AND fiber_g >= :minimum_fiber");
    }
    if (filter.maximumSodiumMg.has_value()) {
        sql += QStringLiteral(" AND sodium_mg <= :maximum_sodium");
    }

    QStringList excludedIds;
    for (const QString& id : filter.excludedIds) {
        const QString normalized = id.trimmed();
        if (!normalized.isEmpty() && !excludedIds.contains(normalized)) {
            excludedIds.append(normalized);
        }
    }
    if (!excludedIds.isEmpty()) {
        QStringList placeholders;
        for (qsizetype index = 0; index < excludedIds.size(); ++index) {
            placeholders.append(QStringLiteral(":excluded_%1").arg(index));
        }
        sql += QStringLiteral(" AND id NOT IN (%1)")
            .arg(placeholders.join(QLatin1Char(',')));
    }

    if (filter.targetCalories.has_value()) {
        sql += QStringLiteral(
            " ORDER BY ABS(total_calories - :target_calories), "
            "protein_g DESC, fiber_g DESC, name");
    } else {
        sql += QStringLiteral(" ORDER BY meal_type, total_calories, name");
    }

    // Nutrition tags are stored as JSON and checked after decoding. Do not
    // apply SQL LIMIT first in that case, otherwise valid tagged rows beyond
    // the first page could be lost.
    const bool applySqlLimit = filter.limit > 0
        && filter.requiredNutritionTags.isEmpty();
    if (applySqlLimit) sql += QStringLiteral(" LIMIT :limit");

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
    if (filter.minimumProteinG.has_value()) {
        query.bindValue(QStringLiteral(":minimum_protein"), *filter.minimumProteinG);
    }
    if (filter.maximumProteinG.has_value()) {
        query.bindValue(QStringLiteral(":maximum_protein"), *filter.maximumProteinG);
    }
    if (filter.minimumCarbohydrateG.has_value()) {
        query.bindValue(
            QStringLiteral(":minimum_carbohydrate"),
            *filter.minimumCarbohydrateG);
    }
    if (filter.maximumCarbohydrateG.has_value()) {
        query.bindValue(
            QStringLiteral(":maximum_carbohydrate"),
            *filter.maximumCarbohydrateG);
    }
    if (filter.minimumFatG.has_value()) {
        query.bindValue(QStringLiteral(":minimum_fat"), *filter.minimumFatG);
    }
    if (filter.maximumFatG.has_value()) {
        query.bindValue(QStringLiteral(":maximum_fat"), *filter.maximumFatG);
    }
    if (filter.minimumFiberG.has_value()) {
        query.bindValue(QStringLiteral(":minimum_fiber"), *filter.minimumFiberG);
    }
    if (filter.maximumSodiumMg.has_value()) {
        query.bindValue(QStringLiteral(":maximum_sodium"), *filter.maximumSodiumMg);
    }
    for (qsizetype index = 0; index < excludedIds.size(); ++index) {
        query.bindValue(
            QStringLiteral(":excluded_%1").arg(index),
            excludedIds.at(index));
    }
    if (filter.targetCalories.has_value()) {
        query.bindValue(
            QStringLiteral(":target_calories"), *filter.targetCalories);
    }
    if (applySqlLimit) {
        query.bindValue(QStringLiteral(":limit"), filter.limit);
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
            if (!applySqlLimit
                && filter.limit > 0
                && recipes.size() >= filter.limit) {
                break;
            }
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
        "nutrition_json, servings, protein_g, carbohydrate_g, fat_g, "
        "saturated_fat_g, fiber_g, sugar_g, sodium_mg, cholesterol_mg, "
        "meal_type, nutrition_tags_json"
        ") VALUES ("
        ":id, :name, :ingredients, :calories, "
        ":nutrition, :servings, :protein, :carbohydrate, :fat, "
        ":saturated_fat, :fiber, :sugar, :sodium, :cholesterol, "
        ":meal_type, :tags"
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
        "protein_g = :protein, "
        "carbohydrate_g = :carbohydrate, "
        "fat_g = :fat, "
        "saturated_fat_g = :saturated_fat, "
        "fiber_g = :fiber, "
        "sugar_g = :sugar, "
        "sodium_mg = :sodium, "
        "cholesterol_mg = :cholesterol, "
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
