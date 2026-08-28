#include "application/csvdataexchangeservice.h"

#include <QCryptographicHash>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace {

struct ParsedCsvLine {
    bool ok = true;
    QStringList fields;
    QString error;
};

ParsedCsvLine parseCsvLine(const QString& line)
{
    ParsedCsvLine result;
    QString field;
    bool insideQuotes = false;

    for (qsizetype index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);
        if (character == QLatin1Char('"')) {
            if (insideQuotes
                && index + 1 < line.size()
                && line.at(index + 1) == QLatin1Char('"')) {
                field.append(QLatin1Char('"'));
                ++index;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (character == QLatin1Char(',') && !insideQuotes) {
            result.fields.append(field.trimmed());
            field.clear();
        } else {
            field.append(character);
        }
    }

    if (insideQuotes) {
        result.ok = false;
        result.error = QStringLiteral("存在未闭合的英文双引号");
        return result;
    }

    result.fields.append(field.trimmed());
    return result;
}

QString normalizedHeader(QString value)
{
    value.remove(QChar::ByteOrderMark);
    return value.trimmed().toLower()
        .replace(QLatin1Char(' '), QLatin1Char('_'))
        .replace(QLatin1Char('-'), QLatin1Char('_'));
}

QHash<QString, int> headerIndex(const QStringList& headers)
{
    QHash<QString, int> indexes;
    for (qsizetype index = 0; index < headers.size(); ++index) {
        indexes.insert(normalizedHeader(headers.at(index)),
                       static_cast<int>(index));
    }
    return indexes;
}

int findColumn(const QHash<QString, int>& indexes,
               std::initializer_list<const char*> names)
{
    for (const char* name : names) {
        const auto iterator = indexes.constFind(QString::fromLatin1(name));
        if (iterator != indexes.constEnd()) return iterator.value();
    }
    return -1;
}

QString fieldAt(const QStringList& fields, int index)
{
    return index >= 0 && index < fields.size()
        ? fields.at(index).trimmed()
        : QString{};
}

ServiceResult<QStringList> readCsvLines(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return ServiceResult<QStringList>::failure(
            QStringLiteral("DATASET_OPEN_ERROR"),
            QStringLiteral("无法打开数据集：%1").arg(file.errorString()));
    }

    QString content = QString::fromUtf8(file.readAll());
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QStringList lines = content.split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) {
        lines.removeLast();
    }
    if (lines.isEmpty()) {
        return ServiceResult<QStringList>::failure(
            QStringLiteral("EMPTY_DATASET"),
            QStringLiteral("CSV 文件中没有数据。"));
    }
    return ServiceResult<QStringList>::success(lines);
}

std::optional<ExerciseCategory> exerciseCategoryFromDataset(
    const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized.isEmpty()
        || normalized == QStringLiteral("other")
        || normalized == QStringLiteral("其他")) {
        return ExerciseCategory::Other;
    }
    if (normalized == QStringLiteral("aerobic")
        || normalized == QStringLiteral("cardio")
        || normalized == QStringLiteral("有氧")) {
        return ExerciseCategory::Aerobic;
    }
    if (normalized == QStringLiteral("strength")
        || normalized == QStringLiteral("resistance")
        || normalized == QStringLiteral("力量")
        || normalized == QStringLiteral("抗阻")) {
        return ExerciseCategory::Strength;
    }
    if (normalized == QStringLiteral("flexibility")
        || normalized == QStringLiteral("柔韧")) {
        return ExerciseCategory::Flexibility;
    }
    if (normalized == QStringLiteral("balance")
        || normalized == QStringLiteral("平衡")) {
        return ExerciseCategory::Balance;
    }
    return std::nullopt;
}

std::optional<MealType> mealTypeFromDataset(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("breakfast")
        || normalized == QStringLiteral("早餐")) {
        return MealType::Breakfast;
    }
    if (normalized == QStringLiteral("lunch")
        || normalized == QStringLiteral("午餐")
        || normalized == QStringLiteral("中餐")) {
        return MealType::Lunch;
    }
    if (normalized == QStringLiteral("dinner")
        || normalized == QStringLiteral("晚餐")) {
        return MealType::Dinner;
    }
    if (normalized == QStringLiteral("snack")
        || normalized == QStringLiteral("加餐")
        || normalized == QStringLiteral("零食")) {
        return MealType::Snack;
    }
    return std::nullopt;
}

QString stableDatasetId(const QString& prefix, const QString& source)
{
    const QByteArray digest = QCryptographicHash::hash(
        source.trimmed().toLower().toUtf8(),
        QCryptographicHash::Sha1).toHex().left(12).toUpper();
    return prefix + QString::fromLatin1(digest);
}

QStringList splitTags(const QString& value)
{
    QStringList tags = value.split(
        QRegularExpression(QStringLiteral("[|,，、]")),
        Qt::SkipEmptyParts);
    for (QString& tag : tags) tag = tag.trimmed();
    tags.removeAll(QString{});
    tags.removeDuplicates();
    return tags;
}

ServiceResult<QVector<Ingredient>> parseIngredients(const QString& value)
{
    QVector<Ingredient> ingredients;
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return ServiceResult<QVector<Ingredient>>::success(ingredients);
    }

    if (trimmed.startsWith(QLatin1Char('['))) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            trimmed.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isArray()) {
            return ServiceResult<QVector<Ingredient>>::failure(
                QStringLiteral("INVALID_INGREDIENTS"),
                QStringLiteral("食材 JSON 格式错误"));
        }
        for (const QJsonValue& valueItem : document.array()) {
            if (!valueItem.isObject()) continue;
            const QJsonObject object = valueItem.toObject();
            Ingredient ingredient;
            ingredient.name = object.value(QStringLiteral("name"))
                                  .toString().trimmed();
            ingredient.amount = object.value(QStringLiteral("amount"))
                                    .toDouble();
            ingredient.unit = object.value(QStringLiteral("unit"))
                                  .toString().trimmed();
            if (!ingredient.name.isEmpty()) ingredients.append(ingredient);
        }
        return ServiceResult<QVector<Ingredient>>::success(ingredients);
    }

    const QStringList parts = trimmed.split(QLatin1Char('|'),
                                            Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QStringList components = part.split(QLatin1Char(':'));
        Ingredient ingredient;
        ingredient.name = components.value(0).trimmed();
        if (ingredient.name.isEmpty()) continue;
        if (components.size() >= 2 && !components.at(1).trimmed().isEmpty()) {
            bool amountOk = false;
            ingredient.amount = components.at(1).trimmed().toDouble(&amountOk);
            if (!amountOk || ingredient.amount < 0.0) {
                return ServiceResult<QVector<Ingredient>>::failure(
                    QStringLiteral("INVALID_INGREDIENTS"),
                    QStringLiteral("食材数量必须是非负数字"));
            }
        }
        if (components.size() >= 3) {
            ingredient.unit = components.at(2).trimmed();
        }
        ingredients.append(ingredient);
    }
    return ServiceResult<QVector<Ingredient>>::success(ingredients);
}

template<typename T>
ServiceResult<ImportBatch<T>> unsupportedImport()
{
    return ServiceResult<ImportBatch<T>>::failure(
        QStringLiteral("UNSUPPORTED_DATA_OPERATION"),
        QStringLiteral("当前基础版本只开放运动和食谱 CSV 导入。"));
}

ServiceResult<bool> unsupportedExport()
{
    return ServiceResult<bool>::failure(
        QStringLiteral("UNSUPPORTED_DATA_OPERATION"),
        QStringLiteral("当前基础版本尚未开放数据导出。"));
}

} // namespace

ServiceResult<ImportBatch<UserProfile>> CsvDataExchangeService::importUsers(
    const QString&,
    DataFormat) const
{
    return unsupportedImport<UserProfile>();
}

ServiceResult<ImportBatch<Exercise>> CsvDataExchangeService::importExercises(
    const QString& filePath,
    DataFormat format) const
{
    if (format != DataFormat::Csv) {
        return ServiceResult<ImportBatch<Exercise>>::failure(
            QStringLiteral("UNSUPPORTED_FORMAT"),
            QStringLiteral("运动数据当前只支持 CSV 格式。"));
    }

    const auto linesResult = readCsvLines(filePath);
    if (!linesResult.ok) {
        return ServiceResult<ImportBatch<Exercise>>::failure(
            linesResult.code, linesResult.message);
    }

    const ParsedCsvLine headerLine = parseCsvLine(linesResult.data.first());
    if (!headerLine.ok) {
        return ServiceResult<ImportBatch<Exercise>>::failure(
            QStringLiteral("INVALID_CSV_HEADER"), headerLine.error);
    }
    const auto indexes = headerIndex(headerLine.fields);
    const int idColumn = findColumn(indexes, {"id", "exercise_id", "code"});
    const int nameColumn = findColumn(indexes, {"name", "exercise", "activity"});
    const int metColumn = findColumn(indexes, {"met_value", "met", "mets"});
    const int categoryColumn = findColumn(indexes, {"category", "type"});
    const int descriptionColumn = findColumn(
        indexes, {"description", "details", "detail"});
    if (nameColumn < 0 || metColumn < 0) {
        return ServiceResult<ImportBatch<Exercise>>::failure(
            QStringLiteral("MISSING_REQUIRED_COLUMNS"),
            QStringLiteral("运动 CSV 至少需要 name 和 met_value 两列。"));
    }

    ImportBatch<Exercise> batch;
    QSet<QString> usedIds;
    for (qsizetype lineIndex = 1;
         lineIndex < linesResult.data.size();
         ++lineIndex) {
        const QString rawLine = linesResult.data.at(lineIndex);
        if (rawLine.trimmed().isEmpty()) continue;
        const int displayLine = static_cast<int>(lineIndex + 1);
        const ParsedCsvLine parsed = parseCsvLine(rawLine);
        if (!parsed.ok) {
            ++batch.skippedRows;
            batch.rowMessages.append(
                QStringLiteral("第 %1 行：%2").arg(displayLine).arg(parsed.error));
            continue;
        }

        Exercise exercise;
        exercise.name = fieldAt(parsed.fields, nameColumn);
        bool metOk = false;
        exercise.metValue = fieldAt(parsed.fields, metColumn).toDouble(&metOk);
        const auto category = exerciseCategoryFromDataset(
            fieldAt(parsed.fields, categoryColumn));
        exercise.description = fieldAt(parsed.fields, descriptionColumn);
        exercise.id = fieldAt(parsed.fields, idColumn);
        if (exercise.id.isEmpty()) {
            exercise.id = stableDatasetId(
                QStringLiteral("EXD_"),
                exercise.name + QLatin1Char('|')
                    + QString::number(exercise.metValue, 'g', 12));
        }

        QString reason;
        if (exercise.name.isEmpty()) reason = QStringLiteral("运动名称为空");
        else if (!metOk || exercise.metValue <= 0.0) {
            reason = QStringLiteral("MET 必须是大于 0 的数字");
        } else if (!category.has_value()) {
            reason = QStringLiteral("无法识别运动分类");
        } else if (usedIds.contains(exercise.id)) {
            reason = QStringLiteral("文件内运动编号重复");
        }
        if (!reason.isEmpty()) {
            ++batch.skippedRows;
            batch.rowMessages.append(
                QStringLiteral("第 %1 行：%2").arg(displayLine).arg(reason));
            continue;
        }

        exercise.category = *category;
        usedIds.insert(exercise.id);
        batch.items.append(exercise);
        ++batch.importedRows;
    }

    return ServiceResult<ImportBatch<Exercise>>::success(
        batch,
        QStringLiteral("运动 CSV 解析完成。"),
        batch.rowMessages);
}

ServiceResult<ImportBatch<Recipe>> CsvDataExchangeService::importRecipes(
    const QString& filePath,
    DataFormat format) const
{
    if (format != DataFormat::Csv) {
        return ServiceResult<ImportBatch<Recipe>>::failure(
            QStringLiteral("UNSUPPORTED_FORMAT"),
            QStringLiteral("食谱数据当前只支持 CSV 格式。"));
    }

    const auto linesResult = readCsvLines(filePath);
    if (!linesResult.ok) {
        return ServiceResult<ImportBatch<Recipe>>::failure(
            linesResult.code, linesResult.message);
    }

    const ParsedCsvLine headerLine = parseCsvLine(linesResult.data.first());
    if (!headerLine.ok) {
        return ServiceResult<ImportBatch<Recipe>>::failure(
            QStringLiteral("INVALID_CSV_HEADER"), headerLine.error);
    }
    const auto indexes = headerIndex(headerLine.fields);
    const int idColumn = findColumn(indexes, {"id", "recipe_id", "code"});
    const int nameColumn = findColumn(indexes, {"name", "recipe_name", "title"});
    const int calorieColumn = findColumn(
        indexes, {"total_calories", "calories", "calories_kcal", "kcal"});
    const int mealColumn = findColumn(indexes, {"meal_type", "meal"});
    const int ingredientsColumn = findColumn(
        indexes, {"ingredients", "ingredients_json"});
    const int tagsColumn = findColumn(
        indexes, {"nutrition_tags", "nutrition_tags_json", "tags"});
    if (nameColumn < 0 || calorieColumn < 0 || mealColumn < 0) {
        return ServiceResult<ImportBatch<Recipe>>::failure(
            QStringLiteral("MISSING_REQUIRED_COLUMNS"),
            QStringLiteral(
                "食谱 CSV 至少需要 name、total_calories 和 meal_type 三列。"));
    }

    ImportBatch<Recipe> batch;
    QSet<QString> usedIds;
    for (qsizetype lineIndex = 1;
         lineIndex < linesResult.data.size();
         ++lineIndex) {
        const QString rawLine = linesResult.data.at(lineIndex);
        if (rawLine.trimmed().isEmpty()) continue;
        const int displayLine = static_cast<int>(lineIndex + 1);
        const ParsedCsvLine parsed = parseCsvLine(rawLine);
        if (!parsed.ok) {
            ++batch.skippedRows;
            batch.rowMessages.append(
                QStringLiteral("第 %1 行：%2").arg(displayLine).arg(parsed.error));
            continue;
        }

        Recipe recipe;
        recipe.name = fieldAt(parsed.fields, nameColumn);
        bool caloriesOk = false;
        recipe.totalCalories = fieldAt(parsed.fields, calorieColumn)
                                    .toDouble(&caloriesOk);
        const auto mealType = mealTypeFromDataset(
            fieldAt(parsed.fields, mealColumn));
        const auto ingredientsResult = parseIngredients(
            fieldAt(parsed.fields, ingredientsColumn));
        recipe.nutritionTags = splitTags(fieldAt(parsed.fields, tagsColumn));
        recipe.id = fieldAt(parsed.fields, idColumn);
        if (recipe.id.isEmpty()) {
            recipe.id = stableDatasetId(
                QStringLiteral("RD_"),
                recipe.name + QLatin1Char('|')
                    + QString::number(recipe.totalCalories, 'g', 12)
                    + QLatin1Char('|') + fieldAt(parsed.fields, mealColumn));
        }

        QString reason;
        if (recipe.name.isEmpty()) reason = QStringLiteral("食谱名称为空");
        else if (!caloriesOk || recipe.totalCalories < 0.0) {
            reason = QStringLiteral("总热量必须是非负数字");
        } else if (!mealType.has_value()) {
            reason = QStringLiteral("无法识别餐别");
        } else if (!ingredientsResult.ok) {
            reason = ingredientsResult.message;
        } else if (usedIds.contains(recipe.id)) {
            reason = QStringLiteral("文件内食谱编号重复");
        }
        if (!reason.isEmpty()) {
            ++batch.skippedRows;
            batch.rowMessages.append(
                QStringLiteral("第 %1 行：%2").arg(displayLine).arg(reason));
            continue;
        }

        recipe.mealType = *mealType;
        recipe.ingredients = ingredientsResult.data;
        usedIds.insert(recipe.id);
        batch.items.append(recipe);
        ++batch.importedRows;
    }

    return ServiceResult<ImportBatch<Recipe>>::success(
        batch,
        QStringLiteral("食谱 CSV 解析完成。"),
        batch.rowMessages);
}

ServiceResult<bool> CsvDataExchangeService::exportUsers(
    const QVector<UserProfile>&,
    const QString&,
    DataFormat) const
{
    return unsupportedExport();
}

ServiceResult<bool> CsvDataExchangeService::exportExercises(
    const QVector<Exercise>&,
    const QString&,
    DataFormat) const
{
    return unsupportedExport();
}

ServiceResult<bool> CsvDataExchangeService::exportRecipes(
    const QVector<Recipe>&,
    const QString&,
    DataFormat) const
{
    return unsupportedExport();
}

ServiceResult<bool> CsvDataExchangeService::exportWeeklyPlan(
    const WeeklyPlan&,
    const QString&,
    DataFormat) const
{
    return unsupportedExport();
}
