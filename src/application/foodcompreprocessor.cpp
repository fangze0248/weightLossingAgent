#include "application/foodcompreprocessor.h"

#include "database/modeljsoncodec.h"
#include "interfaces/IDataExchangeService.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

struct ScoredRecipe {
    Recipe recipe;
    double score = 0.0;
};

int mealIndex(MealType mealType)
{
    return static_cast<int>(mealType);
}

double calorieTarget(MealType mealType)
{
    switch (mealType) {
    case MealType::Breakfast: return 400.0;
    case MealType::Lunch: return 600.0;
    case MealType::Dinner: return 600.0;
    case MealType::Snack: return 250.0;
    }
    return 500.0;
}

double recommendationScore(const Recipe& recipe)
{
    const NutritionFacts& nutrition = recipe.nutritionPerServing;
    const double calorieDistance = std::abs(
        nutrition.caloriesKcal - calorieTarget(recipe.mealType));
    return nutrition.proteinG * 2.0
        + nutrition.fiberG * 1.5
        - calorieDistance * 0.02
        - nutrition.saturatedFatG * 0.3
        - nutrition.sodiumMg * 0.0005;
}

void sortByRecommendationScore(QVector<ScoredRecipe>* recipes)
{
    if (!recipes) return;
    std::sort(
        recipes->begin(), recipes->end(),
        [](const ScoredRecipe& left, const ScoredRecipe& right) {
            if (left.score != right.score) return left.score > right.score;
            return left.recipe.name.compare(
                right.recipe.name, Qt::CaseInsensitive) < 0;
        });
}

bool passesQualityFilter(const Recipe& recipe,
                         const FoodComPreprocessOptions& options)
{
    const NutritionFacts& nutrition = recipe.nutritionPerServing;
    const int ingredientCount = recipe.ingredients.size();
    const bool hasMacronutrients = nutrition.proteinG > 0.0
        || nutrition.carbohydrateG > 0.0
        || nutrition.fatG > 0.0;
    return nutrition.caloriesKcal >= options.minimumCalories
        && nutrition.caloriesKcal <= options.maximumCalories
        && ingredientCount >= options.minimumIngredients
        && ingredientCount <= options.maximumIngredients
        && nutrition.sodiumMg <= options.maximumSodiumMg
        && hasMacronutrients;
}

QString csvCell(QString value)
{
    const bool requiresQuotes = value.contains(QLatin1Char(','))
        || value.contains(QLatin1Char('"'))
        || value.contains(QLatin1Char('\n'))
        || value.contains(QLatin1Char('\r'));
    if (!requiresQuotes) return value;
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}

QString recipeCsvRow(const Recipe& recipe)
{
    const NutritionFacts& nutrition = recipe.nutritionPerServing;
    const QStringList fields = {
        recipe.id,
        recipe.name,
        QString::number(nutrition.caloriesKcal, 'g', 12),
        QString::number(nutrition.proteinG, 'g', 12),
        QString::number(nutrition.carbohydrateG, 'g', 12),
        QString::number(nutrition.fatG, 'g', 12),
        QString::number(nutrition.saturatedFatG, 'g', 12),
        QString::number(nutrition.fiberG, 'g', 12),
        QString::number(nutrition.sugarG, 'g', 12),
        QString::number(nutrition.sodiumMg, 'g', 12),
        QString::number(nutrition.cholesterolMg, 'g', 12),
        QString::number(recipe.servings),
        toStorageString(recipe.mealType),
        model_json_codec::ingredientsToJson(recipe.ingredients),
        model_json_codec::stringListToJson(recipe.nutritionTags)
    };

    QStringList escaped;
    escaped.reserve(fields.size());
    for (const QString& field : fields) escaped.append(csvCell(field));
    return escaped.join(QLatin1Char(','));
}

} // namespace

FoodComPreprocessor::FoodComPreprocessor(
    const IDataExchangeService& dataExchangeService)
    : dataExchangeService_(dataExchangeService)
{
}

ServiceResult<FoodComPreprocessSummary> FoodComPreprocessor::preprocess(
    const QString& inputPath,
    const QString& outputPath,
    const FoodComPreprocessOptions& options) const
{
    if (options.maximumRecipesPerMeal <= 0
        || options.minimumCalories < 0.0
        || options.maximumCalories < options.minimumCalories
        || options.minimumIngredients < 1
        || options.maximumIngredients < options.minimumIngredients) {
        return ServiceResult<FoodComPreprocessSummary>::failure(
            QStringLiteral("INVALID_PREPROCESS_OPTIONS"),
            QStringLiteral("数据集预处理参数无效。"));
    }

    std::array<QVector<ScoredRecipe>, 4> buckets;
    const int retainedPerMeal = qMax(
        options.maximumRecipesPerMeal * 4, 1000);
    const auto streamed = dataExchangeService_.streamRecipes(
        inputPath,
        DataFormat::Csv,
        [&](const Recipe& recipe) {
        if (!passesQualityFilter(recipe, options)) {
            return;
        }
        const int index = mealIndex(recipe.mealType);
        if (index < 0 || index >= static_cast<int>(buckets.size())) {
            return;
        }
        QVector<ScoredRecipe>& bucket = buckets.at(
            static_cast<std::size_t>(index));
        bucket.append(
            ScoredRecipe{recipe, recommendationScore(recipe)});
        if (bucket.size() > retainedPerMeal * 2) {
            sortByRecommendationScore(&bucket);
            bucket.resize(retainedPerMeal);
        }
    });
    if (!streamed.ok) {
        return ServiceResult<FoodComPreprocessSummary>::failure(
            streamed.code, streamed.message, streamed.warnings);
    }

    QVector<Recipe> selected;
    QSet<QString> usedIds;
    QSet<QString> usedNames;
    for (QVector<ScoredRecipe>& bucket : buckets) {
        sortByRecommendationScore(&bucket);

        int mealCount = 0;
        for (const ScoredRecipe& candidate : bucket) {
            if (mealCount >= options.maximumRecipesPerMeal) break;
            const QString normalizedId = candidate.recipe.id.trimmed().toLower();
            const QString normalizedName = candidate.recipe.name.trimmed().toLower();
            if (usedIds.contains(normalizedId)
                || usedNames.contains(normalizedName)) {
                continue;
            }
            usedIds.insert(normalizedId);
            usedNames.insert(normalizedName);
            selected.append(candidate.recipe);
            ++mealCount;
        }
    }

    if (selected.isEmpty()) {
        return ServiceResult<FoodComPreprocessSummary>::failure(
            QStringLiteral("NO_RECIPES_SELECTED"),
            QStringLiteral("没有食谱通过预处理筛选条件。"));
    }

    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        return ServiceResult<FoodComPreprocessSummary>::failure(
            QStringLiteral("OUTPUT_DIRECTORY_ERROR"),
            QStringLiteral("无法创建输出目录。"));
    }

    QSaveFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly)) {
        return ServiceResult<FoodComPreprocessSummary>::failure(
            QStringLiteral("OUTPUT_OPEN_ERROR"), output.errorString());
    }

    QByteArray contents = QByteArrayLiteral(
        "id,name,total_calories,protein_g,carbohydrate_g,fat_g,"
        "saturated_fat_g,fiber_g,sugar_g,sodium_mg,cholesterol_mg,"
        "servings,meal_type,ingredients_json,nutrition_tags_json\n");
    for (const Recipe& recipe : selected) {
        contents.append(recipeCsvRow(recipe).toUtf8());
        contents.append('\n');
    }

    if (output.write(contents) != contents.size() || !output.commit()) {
        return ServiceResult<FoodComPreprocessSummary>::failure(
            QStringLiteral("OUTPUT_WRITE_ERROR"), output.errorString());
    }

    FoodComPreprocessSummary summary;
    summary.parsedRows = streamed.data.importedRows;
    summary.malformedRows = streamed.data.skippedRows;
    summary.filteredRows = streamed.data.importedRows - selected.size();
    summary.selectedRows = selected.size();
    summary.outputPath = outputInfo.absoluteFilePath();
    return ServiceResult<FoodComPreprocessSummary>::success(
        summary,
        QStringLiteral("Food.com 数据集预处理完成。"),
        streamed.data.rowMessages);
}
