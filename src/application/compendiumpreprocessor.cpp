#include "application/compendiumpreprocessor.h"

#include "interfaces/IDataExchangeService.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <array>

namespace {

constexpr int kRecommendationCategoryCount = 4;
constexpr int kIntensityBandCount = 3;

int categoryIndex(ExerciseCategory category)
{
    switch (category) {
    case ExerciseCategory::Aerobic: return 0;
    case ExerciseCategory::Strength: return 1;
    case ExerciseCategory::Flexibility: return 2;
    case ExerciseCategory::Balance: return 3;
    case ExerciseCategory::Other: return -1;
    }
    return -1;
}

int intensityBand(double metValue)
{
    if (metValue < 4.0) return 0;
    if (metValue < 6.0) return 1;
    return 2;
}

QString normalizedCompendiumId(QString sourceId)
{
    sourceId = sourceId.trimmed().toUpper();
    sourceId.replace(
        QRegularExpression(QStringLiteral("[^A-Z0-9]+")),
        QStringLiteral("_"));
    sourceId.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    if (sourceId.startsWith(QStringLiteral("CPA_"))) return sourceId;
    return QStringLiteral("CPA_") + sourceId;
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

QString exerciseCsvRow(const Exercise& exercise)
{
    const QStringList fields = {
        exercise.id,
        exercise.name,
        QString::number(exercise.metValue, 'g', 12),
        toStorageString(exercise.category),
        exercise.description
    };
    QStringList escaped;
    escaped.reserve(fields.size());
    for (const QString& field : fields) escaped.append(csvCell(field));
    return escaped.join(QLatin1Char(','));
}

} // namespace

CompendiumPreprocessor::CompendiumPreprocessor(
    const IDataExchangeService& dataExchangeService)
    : dataExchangeService_(dataExchangeService)
{
}

ServiceResult<CompendiumPreprocessSummary>
CompendiumPreprocessor::preprocess(
    const QString& inputPath,
    const QString& outputPath,
    const CompendiumPreprocessOptions& options) const
{
    if (options.maximumExercisesPerCategory <= 0
        || options.minimumMet <= 0.0
        || options.maximumMet < options.minimumMet) {
        return ServiceResult<CompendiumPreprocessSummary>::failure(
            QStringLiteral("INVALID_PREPROCESS_OPTIONS"),
            QStringLiteral("运动数据集预处理参数无效。"));
    }

    const auto imported = dataExchangeService_.importExercises(
        inputPath, DataFormat::Csv);
    if (!imported.ok) {
        return ServiceResult<CompendiumPreprocessSummary>::failure(
            imported.code, imported.message, imported.warnings);
    }

    using IntensityBuckets =
        std::array<QVector<Exercise>, kIntensityBandCount>;
    std::array<IntensityBuckets, kRecommendationCategoryCount> buckets;
    QSet<QString> usedIds;
    QSet<QString> usedSignatures;
    int filteredRows = 0;

    for (Exercise exercise : imported.data.items) {
        const int category = categoryIndex(exercise.category);
        if (category < 0
            || exercise.metValue < options.minimumMet
            || exercise.metValue > options.maximumMet) {
            ++filteredRows;
            continue;
        }

        exercise.id = normalizedCompendiumId(exercise.id);
        const QString normalizedId = exercise.id.toLower();
        const QString signature = exercise.name.trimmed().toLower()
            + QLatin1Char('|')
            + QString::number(exercise.metValue, 'f', 2);
        if (exercise.id == QStringLiteral("CPA_")
            || usedIds.contains(normalizedId)
            || usedSignatures.contains(signature)) {
            ++filteredRows;
            continue;
        }

        usedIds.insert(normalizedId);
        usedSignatures.insert(signature);
        buckets.at(static_cast<std::size_t>(category))
            .at(static_cast<std::size_t>(intensityBand(exercise.metValue)))
            .append(std::move(exercise));
    }

    QVector<Exercise> selected;
    for (IntensityBuckets& categoryBuckets : buckets) {
        for (QVector<Exercise>& intensityBucket : categoryBuckets) {
            std::sort(
                intensityBucket.begin(), intensityBucket.end(),
                [](const Exercise& left, const Exercise& right) {
                    if (left.metValue != right.metValue) {
                        return left.metValue < right.metValue;
                    }
                    return left.name.compare(
                        right.name, Qt::CaseInsensitive) < 0;
                });
        }

        std::array<qsizetype, kIntensityBandCount> positions{};
        int selectedForCategory = 0;
        bool madeProgress = true;
        while (selectedForCategory < options.maximumExercisesPerCategory
               && madeProgress) {
            madeProgress = false;
            for (int band = 0;
                 band < kIntensityBandCount
                 && selectedForCategory
                     < options.maximumExercisesPerCategory;
                 ++band) {
                QVector<Exercise>& bucket = categoryBuckets.at(
                    static_cast<std::size_t>(band));
                qsizetype& position = positions.at(
                    static_cast<std::size_t>(band));
                if (position >= bucket.size()) continue;
                selected.append(bucket.at(position));
                ++position;
                ++selectedForCategory;
                madeProgress = true;
            }
        }

        for (const QVector<Exercise>& bucket : categoryBuckets) {
            filteredRows += bucket.size();
        }
        filteredRows -= selectedForCategory;
    }

    if (selected.isEmpty()) {
        return ServiceResult<CompendiumPreprocessSummary>::failure(
            QStringLiteral("NO_EXERCISES_SELECTED"),
            QStringLiteral("没有运动通过预处理筛选条件。"));
    }

    const QFileInfo outputInfo(outputPath);
    if (!QDir().mkpath(outputInfo.absolutePath())) {
        return ServiceResult<CompendiumPreprocessSummary>::failure(
            QStringLiteral("OUTPUT_DIRECTORY_ERROR"),
            QStringLiteral("无法创建输出目录。"));
    }

    QSaveFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly)) {
        return ServiceResult<CompendiumPreprocessSummary>::failure(
            QStringLiteral("OUTPUT_OPEN_ERROR"), output.errorString());
    }

    QByteArray contents = QByteArrayLiteral(
        "id,name,met_value,category,description\n");
    for (const Exercise& exercise : selected) {
        contents.append(exerciseCsvRow(exercise).toUtf8());
        contents.append('\n');
    }

    if (output.write(contents) != contents.size() || !output.commit()) {
        return ServiceResult<CompendiumPreprocessSummary>::failure(
            QStringLiteral("OUTPUT_WRITE_ERROR"), output.errorString());
    }

    CompendiumPreprocessSummary summary;
    summary.parsedRows = imported.data.importedRows;
    summary.malformedRows = imported.data.skippedRows;
    summary.filteredRows = filteredRows;
    summary.selectedRows = selected.size();
    summary.outputPath = outputInfo.absoluteFilePath();
    return ServiceResult<CompendiumPreprocessSummary>::success(
        summary,
        QStringLiteral("2024 Compendium 运动数据集预处理完成。"),
        imported.data.rowMessages);
}
