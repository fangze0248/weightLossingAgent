#include "application/builtindatasetinitializer.h"

#include "database/modeljsoncodec.h"
#include "interfaces/IDataExchangeService.h"
#include "interfaces/IExerciseRepository.h"
#include "interfaces/IRecipeRepository.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>

#include <utility>

BuiltinDatasetInitializer::BuiltinDatasetInitializer(
    QSqlDatabase database,
    IRecipeRepository& recipeRepository,
    const IDataExchangeService& dataExchangeService)
    : database_(std::move(database))
    , recipeRepository_(&recipeRepository)
    , dataExchangeService_(dataExchangeService)
{
}

BuiltinDatasetInitializer::BuiltinDatasetInitializer(
    QSqlDatabase database,
    IExerciseRepository& exerciseRepository,
    IRecipeRepository& recipeRepository,
    const IDataExchangeService& dataExchangeService)
    : database_(std::move(database))
    , exerciseRepository_(&exerciseRepository)
    , recipeRepository_(&recipeRepository)
    , dataExchangeService_(dataExchangeService)
{
}

ServiceResult<DatasetInitializationSummary>
BuiltinDatasetInitializer::importExercisesIfChanged(
    const QString& datasetKey,
    const QString& datasetPath)
{
    if (!exerciseRepository_) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("EXERCISE_REPOSITORY_NOT_CONFIGURED"),
            QStringLiteral("内置数据初始化器未配置运动仓库。"));
    }
    if (datasetKey.trimmed().isEmpty()) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("INVALID_DATASET_KEY"),
            QStringLiteral("内置数据集标识不能为空。"));
    }

    QFile datasetFile(datasetPath);
    if (!datasetFile.open(QIODevice::ReadOnly)) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("BUILTIN_DATASET_OPEN_ERROR"),
            QStringLiteral("无法打开内置数据集：%1")
                .arg(datasetFile.errorString()));
    }
    const QByteArray contents = datasetFile.readAll();
    const QString contentHash = QString::fromLatin1(
        QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex());

    QSqlQuery versionQuery(database_);
    versionQuery.prepare(QStringLiteral(
        "SELECT content_hash FROM dataset_imports WHERE dataset_key = :key"));
    versionQuery.bindValue(QStringLiteral(":key"), datasetKey.trimmed());
    if (!versionQuery.exec()) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("DATASET_VERSION_READ_ERROR"),
            versionQuery.lastError().text());
    }
    if (versionQuery.next()
        && versionQuery.value(0).toString() == contentHash) {
        DatasetInitializationSummary summary;
        summary.contentHash = contentHash;
        return ServiceResult<DatasetInitializationSummary>::success(
            summary,
            QStringLiteral("内置运动数据集未变化，无需重复导入。"));
    }

    const auto parsed = dataExchangeService_.importExercises(
        datasetPath, DataFormat::Csv);
    if (!parsed.ok) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            parsed.code, parsed.message, parsed.warnings);
    }
    if (parsed.data.items.isEmpty()) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("EMPTY_BUILTIN_DATASET"),
            QStringLiteral("内置数据集中没有可导入的运动。"),
            parsed.data.rowMessages);
    }

    if (!database_.transaction()) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("DATASET_TRANSACTION_ERROR"),
            database_.lastError().text());
    }

    int storedRows = 0;
    QSqlQuery upsert(database_);
    upsert.prepare(QStringLiteral(
        "INSERT INTO exercises "
        "(id, name, met_value, category, description) "
        "VALUES (:id, :name, :met, :category, :description) "
        "ON CONFLICT(id) DO UPDATE SET "
        "name = excluded.name, met_value = excluded.met_value, "
        "category = excluded.category, description = excluded.description"));
    for (const Exercise& exercise : parsed.data.items) {
        upsert.bindValue(QStringLiteral(":id"), exercise.id.trimmed());
        upsert.bindValue(QStringLiteral(":name"), exercise.name.trimmed());
        upsert.bindValue(QStringLiteral(":met"), exercise.metValue);
        upsert.bindValue(
            QStringLiteral(":category"), toStorageString(exercise.category));
        upsert.bindValue(
            QStringLiteral(":description"), exercise.description);
        if (!upsert.exec()) {
            const QString error = upsert.lastError().text();
            database_.rollback();
            return ServiceResult<DatasetInitializationSummary>::failure(
                QStringLiteral("DATASET_EXERCISE_UPSERT_ERROR"), error);
        }
        ++storedRows;
    }

    QSqlQuery recordVersion(database_);
    recordVersion.prepare(QStringLiteral(
        "INSERT INTO dataset_imports "
        "(dataset_key, content_hash, imported_at, imported_rows) "
        "VALUES (:key, :hash, :imported_at, :rows) "
        "ON CONFLICT(dataset_key) DO UPDATE SET "
        "content_hash = excluded.content_hash, "
        "imported_at = excluded.imported_at, "
        "imported_rows = excluded.imported_rows"));
    recordVersion.bindValue(QStringLiteral(":key"), datasetKey.trimmed());
    recordVersion.bindValue(QStringLiteral(":hash"), contentHash);
    recordVersion.bindValue(
        QStringLiteral(":imported_at"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    recordVersion.bindValue(QStringLiteral(":rows"), storedRows);
    if (!recordVersion.exec()) {
        const QString error = recordVersion.lastError().text();
        database_.rollback();
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("DATASET_VERSION_WRITE_ERROR"), error);
    }

    if (!database_.commit()) {
        const QString error = database_.lastError().text();
        database_.rollback();
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("DATASET_COMMIT_ERROR"), error);
    }

    DatasetInitializationSummary summary;
    summary.imported = true;
    summary.parsedRows = parsed.data.importedRows;
    summary.storedRows = storedRows;
    summary.skippedRows = parsed.data.skippedRows;
    summary.contentHash = contentHash;
    return ServiceResult<DatasetInitializationSummary>::success(
        summary,
        QStringLiteral("内置运动数据集导入完成。"),
        parsed.data.rowMessages);
}

ServiceResult<DatasetInitializationSummary>
BuiltinDatasetInitializer::importRecipesIfChanged(
    const QString& datasetKey,
    const QString& datasetPath)
{
    if (!recipeRepository_) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("RECIPE_REPOSITORY_NOT_CONFIGURED"),
            QStringLiteral("内置数据初始化器未配置食谱仓库。"));
    }
    if (datasetKey.trimmed().isEmpty()) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("INVALID_DATASET_KEY"),
            QStringLiteral("内置数据集标识不能为空。"));
    }

    QFile datasetFile(datasetPath);
    if (!datasetFile.open(QIODevice::ReadOnly)) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("BUILTIN_DATASET_OPEN_ERROR"),
            QStringLiteral("无法打开内置数据集：%1")
                .arg(datasetFile.errorString()));
    }
    const QByteArray contents = datasetFile.readAll();
    const QString contentHash = QString::fromLatin1(
        QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex());

    QSqlQuery versionQuery(database_);
    versionQuery.prepare(QStringLiteral(
        "SELECT content_hash FROM dataset_imports WHERE dataset_key = :key"));
    versionQuery.bindValue(QStringLiteral(":key"), datasetKey.trimmed());
    if (!versionQuery.exec()) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("DATASET_VERSION_READ_ERROR"),
            versionQuery.lastError().text());
    }
    if (versionQuery.next()
        && versionQuery.value(0).toString() == contentHash) {
        DatasetInitializationSummary summary;
        summary.contentHash = contentHash;
        return ServiceResult<DatasetInitializationSummary>::success(
            summary,
            QStringLiteral("内置食谱数据集未变化，无需重复导入。"));
    }

    const auto parsed = dataExchangeService_.importRecipes(
        datasetPath, DataFormat::Csv);
    if (!parsed.ok) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            parsed.code, parsed.message, parsed.warnings);
    }
    if (parsed.data.items.isEmpty()) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("EMPTY_BUILTIN_DATASET"),
            QStringLiteral("内置数据集中没有可导入的食谱。"),
            parsed.data.rowMessages);
    }

    if (!database_.transaction()) {
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("DATASET_TRANSACTION_ERROR"),
            database_.lastError().text());
    }

    int storedRows = 0;
    QSqlQuery upsert(database_);
    upsert.prepare(QStringLiteral(
        "INSERT INTO recipes ("
        "id, name, ingredients_json, total_calories, nutrition_json, "
        "servings, protein_g, carbohydrate_g, fat_g, saturated_fat_g, "
        "fiber_g, sugar_g, sodium_mg, cholesterol_mg, meal_type, "
        "nutrition_tags_json) VALUES ("
        ":id, :name, :ingredients, :calories, :nutrition, :servings, "
        ":protein, :carbohydrate, :fat, :saturated_fat, :fiber, :sugar, "
        ":sodium, :cholesterol, :meal_type, :tags) "
        "ON CONFLICT(id) DO UPDATE SET "
        "name = excluded.name, ingredients_json = excluded.ingredients_json, "
        "total_calories = excluded.total_calories, "
        "nutrition_json = excluded.nutrition_json, "
        "servings = excluded.servings, protein_g = excluded.protein_g, "
        "carbohydrate_g = excluded.carbohydrate_g, fat_g = excluded.fat_g, "
        "saturated_fat_g = excluded.saturated_fat_g, "
        "fiber_g = excluded.fiber_g, sugar_g = excluded.sugar_g, "
        "sodium_mg = excluded.sodium_mg, "
        "cholesterol_mg = excluded.cholesterol_mg, "
        "meal_type = excluded.meal_type, "
        "nutrition_tags_json = excluded.nutrition_tags_json"));
    for (const Recipe& recipe : parsed.data.items) {
        NutritionFacts nutrition = recipe.nutritionPerServing;
        if (nutrition.caloriesKcal <= 0.0) {
            nutrition.caloriesKcal = recipe.totalCalories;
        }
        upsert.bindValue(QStringLiteral(":id"), recipe.id.trimmed());
        upsert.bindValue(QStringLiteral(":name"), recipe.name.trimmed());
        upsert.bindValue(
            QStringLiteral(":ingredients"),
            model_json_codec::ingredientsToJson(recipe.ingredients));
        upsert.bindValue(
            QStringLiteral(":calories"), nutrition.caloriesKcal);
        upsert.bindValue(
            QStringLiteral(":nutrition"),
            model_json_codec::nutritionFactsToJson(nutrition));
        upsert.bindValue(QStringLiteral(":servings"), recipe.servings);
        upsert.bindValue(QStringLiteral(":protein"), nutrition.proteinG);
        upsert.bindValue(
            QStringLiteral(":carbohydrate"), nutrition.carbohydrateG);
        upsert.bindValue(QStringLiteral(":fat"), nutrition.fatG);
        upsert.bindValue(
            QStringLiteral(":saturated_fat"), nutrition.saturatedFatG);
        upsert.bindValue(QStringLiteral(":fiber"), nutrition.fiberG);
        upsert.bindValue(QStringLiteral(":sugar"), nutrition.sugarG);
        upsert.bindValue(QStringLiteral(":sodium"), nutrition.sodiumMg);
        upsert.bindValue(
            QStringLiteral(":cholesterol"), nutrition.cholesterolMg);
        upsert.bindValue(
            QStringLiteral(":meal_type"), toStorageString(recipe.mealType));
        upsert.bindValue(
            QStringLiteral(":tags"),
            model_json_codec::stringListToJson(recipe.nutritionTags));
        if (!upsert.exec()) {
            const QString error = upsert.lastError().text();
            database_.rollback();
            return ServiceResult<DatasetInitializationSummary>::failure(
                QStringLiteral("DATASET_RECIPE_UPSERT_ERROR"), error);
        }
        ++storedRows;
    }

    QSqlQuery recordVersion(database_);
    recordVersion.prepare(QStringLiteral(
        "INSERT INTO dataset_imports "
        "(dataset_key, content_hash, imported_at, imported_rows) "
        "VALUES (:key, :hash, :imported_at, :rows) "
        "ON CONFLICT(dataset_key) DO UPDATE SET "
        "content_hash = excluded.content_hash, "
        "imported_at = excluded.imported_at, "
        "imported_rows = excluded.imported_rows"));
    recordVersion.bindValue(QStringLiteral(":key"), datasetKey.trimmed());
    recordVersion.bindValue(QStringLiteral(":hash"), contentHash);
    recordVersion.bindValue(
        QStringLiteral(":imported_at"),
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    recordVersion.bindValue(QStringLiteral(":rows"), storedRows);
    if (!recordVersion.exec()) {
        const QString error = recordVersion.lastError().text();
        database_.rollback();
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("DATASET_VERSION_WRITE_ERROR"), error);
    }

    if (!database_.commit()) {
        const QString error = database_.lastError().text();
        database_.rollback();
        return ServiceResult<DatasetInitializationSummary>::failure(
            QStringLiteral("DATASET_COMMIT_ERROR"), error);
    }

    DatasetInitializationSummary summary;
    summary.imported = true;
    summary.parsedRows = parsed.data.importedRows;
    summary.storedRows = storedRows;
    summary.skippedRows = parsed.data.skippedRows;
    summary.contentHash = contentHash;
    return ServiceResult<DatasetInitializationSummary>::success(
        summary,
        QStringLiteral("内置食谱数据集导入完成。"),
        parsed.data.rowMessages);
}
