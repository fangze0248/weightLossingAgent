#include "application/compendiumpreprocessor.h"
#include "application/csvdataexchangeservice.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(
        QStringLiteral("compendium_preprocessor"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral(
            "筛选 2024 Adult Compendium CSV 并生成内置运动数据集。"));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(
        {QStringLiteral("i"), QStringLiteral("input")},
        QStringLiteral("Compendium CSV 路径。"),
        QStringLiteral("file")));
    parser.addOption(QCommandLineOption(
        {QStringLiteral("o"), QStringLiteral("output")},
        QStringLiteral("输出的标准 CSV 路径。"),
        QStringLiteral("file")));
    parser.addOption(QCommandLineOption(
        {QStringLiteral("m"), QStringLiteral("max-per-category")},
        QStringLiteral("每种运动类别最多保留的条目数，默认 150。"),
        QStringLiteral("count"),
        QStringLiteral("150")));
    parser.process(application);

    const QString inputPath = parser.value(QStringLiteral("input"));
    const QString outputPath = parser.value(QStringLiteral("output"));
    bool countOk = false;
    const int maximumPerCategory = parser.value(
        QStringLiteral("max-per-category")).toInt(&countOk);
    if (inputPath.isEmpty() || outputPath.isEmpty()
        || !countOk || maximumPerCategory <= 0) {
        qCritical().noquote()
            << QStringLiteral(
                   "用法：compendium_preprocessor --input compendium.csv "
                   "--output datasets/builtin/exercises.csv "
                   "[--max-per-category 150]");
        return 1;
    }

    CsvDataExchangeService dataExchangeService;
    CompendiumPreprocessor preprocessor(dataExchangeService);
    CompendiumPreprocessOptions options;
    options.maximumExercisesPerCategory = maximumPerCategory;
    const auto result = preprocessor.preprocess(
        inputPath, outputPath, options);
    if (!result.ok) {
        qCritical().noquote() << result.code << result.message;
        return 2;
    }

    qInfo().noquote()
        << QStringLiteral(
               "预处理完成：解析 %1 条，原始格式跳过 %2 条，"
               "筛选/去重 %3 条，最终输出 %4 条。\n输出：%5")
               .arg(result.data.parsedRows)
               .arg(result.data.malformedRows)
               .arg(result.data.filteredRows)
               .arg(result.data.selectedRows)
               .arg(result.data.outputPath);
    return 0;
}
