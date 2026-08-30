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

    // Split only at record-ending newlines. Newlines inside quoted CSV fields
    // belong to the field and must remain part of the same logical record.
    QStringList lines;
    QString record;
    bool insideQuotes = false;
    for (qsizetype index = 0; index < content.size(); ++index) {
        const QChar character = content.at(index);
        if (character == QLatin1Char('"')) {
            record.append(character);
            if (insideQuotes
                && index + 1 < content.size()
                && content.at(index + 1) == QLatin1Char('"')) {
                record.append(content.at(index + 1));
                ++index;
            } else {
                insideQuotes = !insideQuotes;
            }
        } else if (character == QLatin1Char('\n') && !insideQuotes) {
            if (!record.trimmed().isEmpty()) {
                lines.append(record);
            }
            record.clear();
        } else {
            record.append(character);
        }
    }

    if (insideQuotes) {
        return ServiceResult<QStringList>::failure(
            QStringLiteral("INVALID_CSV"),
            QStringLiteral("CSV 文件末尾存在未闭合的英文双引号。"));
    }
    if (!record.trimmed().isEmpty()) {
        lines.append(record);
    }
    if (lines.isEmpty()) {
        return ServiceResult<QStringList>::failure(
            QStringLiteral("EMPTY_DATASET"),
            QStringLiteral("CSV 文件中没有数据。"));
    }
    return ServiceResult<QStringList>::success(lines);
}

void appendRowMessage(QStringList& messages, const QString& message)
{
    constexpr qsizetype maximumMessages = 100;
    if (messages.size() < maximumMessages) {
        messages.append(message);
    } else if (messages.size() == maximumMessages) {
        messages.append(QStringLiteral("其余格式错误提示已省略。"));
    }
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

std::optional<MealType> mealTypeFromFoodCom(
    const QString& category,
    const QString& keywords)
{
    const QString searchableText =
        category.trimmed().toLower()
        + QLatin1Char('|')
        + keywords.trimmed().toLower();

    const auto containsAny =
        [&searchableText](
            std::initializer_list<const char*> values) {
        for (const char* value : values) {
            if (searchableText.contains(
                    QString::fromLatin1(value))) {
                return true;
            }
        }
        return false;
    };

    // Prefer explicit breakfast and brunch indicators.
    if (containsAny({
            "breakfast",
            "brunch",
            "omelet",
            "pancake"
        })) {
        return MealType::Breakfast;
    }

    if (containsAny({
            "lunch",
            "sandwich",
            "salad"
        })) {
        return MealType::Lunch;
    }

    if (containsAny({
            "snack",
            "dessert",
            "beverage",
            "appetizer",
            "cookie",
            "smoothie"
        })) {
        return MealType::Snack;
    }

    // Main dishes default to dinner when no earlier meal signal matched.
    if (containsAny({
            "dinner",
            "main dish",
            "main-dish",
            "one dish meal",
            "one-dish-meal",
            "chicken",
            "beef",
            "pork",
            "seafood",
            "pasta"
        })) {
        return MealType::Dinner;
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

QStringList parseStringList(const QString& value)
{
    QString text = value.trimmed();
    if (text.isEmpty()
        || text.compare(QStringLiteral("NA"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("NULL"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("character(0)"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("c()"), Qt::CaseInsensitive) == 0) {
        return {};
    }

    if (text.startsWith(QLatin1Char('['))) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            text.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError
            && document.isArray()) {
            QStringList values;
            for (const QJsonValue& item : document.array()) {
                if (item.isString()) {
                    values.append(item.toString().trimmed());
                } else if (item.isDouble()) {
                    values.append(QString::number(item.toDouble(), 'g', 12));
                }
            }
            values.removeAll(QString{});
            return values;
        }
    }

    static const QRegularExpression rVectorPrefix(
        QStringLiteral("^c\\s*\\("),
        QRegularExpression::CaseInsensitiveOption);
    const bool isRVector = rVectorPrefix.match(text).hasMatch()
        && text.endsWith(QLatin1Char(')'));
    if (isRVector) {
        const qsizetype openingParenthesis = text.indexOf(QLatin1Char('('));
        text = text.mid(openingParenthesis + 1,
                        text.size() - openingParenthesis - 2);
    }

    const QRegularExpression plainSeparator(
        QStringLiteral("[|，、]"));
    if (!isRVector && text.contains(plainSeparator)) {
        QStringList values = text.split(plainSeparator, Qt::SkipEmptyParts);
        for (QString& item : values) item = item.trimmed();
        values.removeAll(QString{});
        return values;
    }

    QStringList values;
    QString current;
    QChar quote;
    bool escaped = false;
    for (const QChar character : text) {
        if (escaped) {
            current.append(character);
            escaped = false;
        } else if (!quote.isNull() && character == QLatin1Char('\\')) {
            escaped = true;
        } else if (!quote.isNull() && character == quote) {
            quote = QChar{};
        } else if (quote.isNull()
                   && (character == QLatin1Char('"')
                       || character == QLatin1Char('\''))) {
            quote = character;
        } else if (quote.isNull() && character == QLatin1Char(',')) {
            const QString item = current.trimmed();
            if (!item.isEmpty()) values.append(item);
            current.clear();
        } else {
            current.append(character);
        }
    }

    const QString finalItem = current.trimmed();
    if (!finalItem.isEmpty()) values.append(finalItem);
    if (values.isEmpty() && !text.trimmed().isEmpty()) {
        values.append(text.trimmed());
    }
    values.removeAll(QStringLiteral("NA"));
    return values;
}

QStringList splitTags(const QString& value)
{
    QStringList tags = parseStringList(value);
    for (QString& tag : tags) tag = tag.trimmed();
    tags.removeAll(QString{});
    tags.removeDuplicates();
    return tags;
}

void parseIngredientQuantity(const QString& rawQuantity,
                             Ingredient* ingredient)
{
    if (!ingredient) return;

    QString quantity = rawQuantity.trimmed();
    if (quantity.isEmpty()
        || quantity.compare(QStringLiteral("NA"), Qt::CaseInsensitive) == 0) {
        return;
    }

    quantity.replace(QChar(0x00BC), QStringLiteral(" 1/4"));
    quantity.replace(QChar(0x00BD), QStringLiteral(" 1/2"));
    quantity.replace(QChar(0x00BE), QStringLiteral(" 3/4"));
    quantity.replace(QChar(0x2153), QStringLiteral(" 1/3"));
    quantity.replace(QChar(0x2154), QStringLiteral(" 2/3"));
    quantity.replace(QChar(0x215B), QStringLiteral(" 1/8"));
    quantity.replace(QChar(0x215C), QStringLiteral(" 3/8"));
    quantity.replace(QChar(0x215D), QStringLiteral(" 5/8"));
    quantity.replace(QChar(0x215E), QStringLiteral(" 7/8"));
    quantity = quantity.simplified();

    static const QRegularExpression mixedFraction(
        QStringLiteral(
            "^(\\d+(?:\\.\\d+)?)\\s+(\\d+)\\s*/\\s*(\\d+)(?:\\s+(.*))?$"));
    static const QRegularExpression simpleFraction(
        QStringLiteral("^(\\d+)\\s*/\\s*(\\d+)(?:\\s+(.*))?$"));
    static const QRegularExpression decimalNumber(
        QStringLiteral("^(\\d+(?:\\.\\d+)?)(?:\\s+(.*))?$"));

    QRegularExpressionMatch match = mixedFraction.match(quantity);
    if (match.hasMatch()) {
        const double denominator = match.captured(3).toDouble();
        if (denominator > 0.0) {
            ingredient->amount = match.captured(1).toDouble()
                + match.captured(2).toDouble() / denominator;
            ingredient->unit = match.captured(4).trimmed();
            return;
        }
    }

    match = simpleFraction.match(quantity);
    if (match.hasMatch()) {
        const double denominator = match.captured(2).toDouble();
        if (denominator > 0.0) {
            ingredient->amount = match.captured(1).toDouble() / denominator;
            ingredient->unit = match.captured(3).trimmed();
            return;
        }
    }

    match = decimalNumber.match(quantity);
    if (match.hasMatch()) {
        ingredient->amount = match.captured(1).toDouble();
        ingredient->unit = match.captured(2).trimmed();
        return;
    }

    // Preserve non-numeric quantities such as "to taste" instead of losing
    // source information. Amount remains zero and the original text is kept.
    ingredient->unit = rawQuantity.trimmed();
}

ServiceResult<QVector<Ingredient>> parseFoodComIngredients(
    const QString& namesValue,
    const QString& quantitiesValue)
{
    const QStringList names = parseStringList(namesValue);
    const QStringList quantities = parseStringList(quantitiesValue);
    QVector<Ingredient> ingredients;
    ingredients.reserve(names.size());

    for (qsizetype index = 0; index < names.size(); ++index) {
        Ingredient ingredient;
        ingredient.name = names.at(index).trimmed();
        if (ingredient.name.isEmpty()) continue;
        parseIngredientQuantity(quantities.value(index), &ingredient);
        ingredients.append(ingredient);
    }

    return ServiceResult<QVector<Ingredient>>::success(ingredients);
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
            if (valueItem.isString()) {
                Ingredient ingredient;
                ingredient.name = valueItem.toString().trimmed();
                if (!ingredient.name.isEmpty()) ingredients.append(ingredient);
                continue;
            }
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
            appendRowMessage(
                batch.rowMessages,
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
            appendRowMessage(
                batch.rowMessages,
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
    const int idColumn = findColumn(
        indexes, {"id", "recipe_id", "recipeid", "code"});
    const int nameColumn = findColumn(indexes, {"name", "recipe_name", "title"});
    const int calorieColumn = findColumn(
        indexes, {"total_calories", "calories", "calories_kcal", "kcal"});
    const int proteinColumn = findColumn(
        indexes, {"proteincontent", "protein_content", "protein_g"});

    const int carbohydrateColumn = findColumn(
        indexes, {"carbohydratecontent", "carbohydrate_content", "carbohydrate_g"});

    const int fatColumn = findColumn(
        indexes, {"fatcontent", "fat_content", "fat_g"});

    const int saturatedFatColumn = findColumn(
        indexes, {"saturatedfatcontent", "saturated_fat_content", "saturated_fat_g"});

    const int fiberColumn = findColumn(
        indexes, {"fibercontent", "fiber_content", "fiber_g"});

    const int sugarColumn = findColumn(
        indexes, {"sugarcontent", "sugar_content", "sugar_g"});

    const int sodiumColumn = findColumn(
        indexes, {"sodiumcontent", "sodium_content", "sodium_mg"});

    const int cholesterolColumn = findColumn(
        indexes, {"cholesterolcontent", "cholesterol_content", "cholesterol_mg"});

    const int servingsColumn = findColumn(
        indexes, {"recipeservings", "recipe_servings", "servings"});
    const int mealColumn = findColumn(indexes, {"meal_type", "meal"});
    const int categoryColumn = findColumn(
        indexes, {"recipecategory", "recipe_category", "category"});
    const int keywordsColumn = findColumn(
        indexes, {"keywords", "recipe_keywords"});
    const int ingredientsColumn = findColumn(
        indexes, {"ingredients", "ingredients_json"});
    const int ingredientPartsColumn = findColumn(
        indexes,
        {"recipeingredientparts", "recipe_ingredient_parts", "ingredient_parts"});
    const int ingredientQuantitiesColumn = findColumn(
        indexes,
        {"recipeingredientquantities", "recipe_ingredient_quantities",
         "ingredient_quantities"});
    const int tagsColumn = findColumn(
        indexes, {"nutrition_tags", "nutrition_tags_json", "tags", "keywords"});
    const bool hasMealSource = mealColumn >= 0
        || categoryColumn >= 0
        || keywordsColumn >= 0;
    const bool hasIngredientSource = ingredientsColumn >= 0
        || ingredientPartsColumn >= 0;
    if (nameColumn < 0
        || calorieColumn < 0
        || !hasMealSource
        || !hasIngredientSource) {
        return ServiceResult<ImportBatch<Recipe>>::failure(
            QStringLiteral("MISSING_REQUIRED_COLUMNS"),
            QStringLiteral(
                "食谱 CSV 需要名称、热量、餐别来源和食材来源列。"
                "餐别可以使用 meal_type，或使用 RecipeCategory/Keywords 推断；"
                "食材可以使用 ingredients，或使用 RecipeIngredientParts。"));
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
            appendRowMessage(
                batch.rowMessages,
                QStringLiteral("第 %1 行：%2").arg(displayLine).arg(parsed.error));
            continue;
        }

        Recipe recipe;
        recipe.name = fieldAt(parsed.fields, nameColumn);
        bool caloriesOk = false;
        recipe.totalCalories = fieldAt(parsed.fields, calorieColumn)
                                    .toDouble(&caloriesOk);
        // 旧热量字段和新营养字段保持一致
        recipe.nutritionPerServing.caloriesKcal =
            recipe.totalCalories;

        // 读取一个可选的非负营养数值。
        // 找不到列、内容为空、不是数字或小于0时，暂时返回0。
        const auto readOptionalNutrition =
            [&parsed](int column) -> double {
            bool ok = false;
            const double value =
                fieldAt(parsed.fields, column).toDouble(&ok);

            return ok && value >= 0.0 ? value : 0.0;
        };

        recipe.nutritionPerServing.proteinG =
            readOptionalNutrition(proteinColumn);

        recipe.nutritionPerServing.carbohydrateG =
            readOptionalNutrition(carbohydrateColumn);

        recipe.nutritionPerServing.fatG =
            readOptionalNutrition(fatColumn);

        recipe.nutritionPerServing.saturatedFatG =
            readOptionalNutrition(saturatedFatColumn);

        recipe.nutritionPerServing.fiberG =
            readOptionalNutrition(fiberColumn);

        recipe.nutritionPerServing.sugarG =
            readOptionalNutrition(sugarColumn);

        recipe.nutritionPerServing.sodiumMg =
            readOptionalNutrition(sodiumColumn);

        recipe.nutritionPerServing.cholesterolMg =
            readOptionalNutrition(cholesterolColumn);

        bool servingsOk = false;
        const double importedServings =
            fieldAt(parsed.fields, servingsColumn).toDouble(&servingsOk);
        recipe.servings = servingsOk && importedServings > 0.0
            ? qMax(1, static_cast<int>(importedServings))
            : 1;

        const QString categoryText = fieldAt(parsed.fields, categoryColumn);
        const QString keywordsText = fieldAt(parsed.fields, keywordsColumn);
        auto mealType = mealTypeFromDataset(
            fieldAt(parsed.fields, mealColumn));
        if (!mealType.has_value()) {
            mealType = mealTypeFromFoodCom(categoryText, keywordsText);
        }

        const auto ingredientsResult = ingredientsColumn >= 0
            ? parseIngredients(fieldAt(parsed.fields, ingredientsColumn))
            : parseFoodComIngredients(
                  fieldAt(parsed.fields, ingredientPartsColumn),
                  fieldAt(parsed.fields, ingredientQuantitiesColumn));

        recipe.nutritionTags = splitTags(
            fieldAt(parsed.fields, tagsColumn));
        if (!categoryText.trimmed().isEmpty()
            && !recipe.nutritionTags.contains(
                categoryText.trimmed(), Qt::CaseInsensitive)) {
            recipe.nutritionTags.append(categoryText.trimmed());
        }
        recipe.id = fieldAt(parsed.fields, idColumn);
        if (recipe.id.isEmpty()) {
            recipe.id = stableDatasetId(
                QStringLiteral("RD_"),
                recipe.name + QLatin1Char('|')
                    + QString::number(recipe.totalCalories, 'g', 12)
                    + QLatin1Char('|') + categoryText
                    + QLatin1Char('|') + keywordsText);
        }

        QString reason;
        if (recipe.name.isEmpty()) reason = QStringLiteral("食谱名称为空");
        else if (!caloriesOk || recipe.totalCalories < 0.0) {
            reason = QStringLiteral("总热量必须是非负数字");
        } else if (!mealType.has_value()) {
            reason = QStringLiteral("无法识别餐别");
        } else if (!ingredientsResult.ok) {
            reason = ingredientsResult.message;
        } else if (ingredientsResult.data.isEmpty()) {
            reason = QStringLiteral("食材列表为空");
        } else if (usedIds.contains(recipe.id)) {
            reason = QStringLiteral("文件内食谱编号重复");
        }
        if (!reason.isEmpty()) {
            ++batch.skippedRows;
            appendRowMessage(
                batch.rowMessages,
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
