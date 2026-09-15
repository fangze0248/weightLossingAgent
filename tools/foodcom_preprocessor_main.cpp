#include "application/csvdataexchangeservice.h"
#include "application/foodcompreprocessor.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("foodcom_preprocessor"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("筛选 Food.com CSV 并生成程序内置食谱数据集。"));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(
        {QStringLiteral("i"), QStringLiteral("input")},
        QStringLiteral("Food.com 原始 recipes.csv 路径。"),
        QStringLiteral("file")));
    parser.addOption(QCommandLineOption(
        {QStringLiteral("o"), QStringLiteral("output")},
        QStringLiteral("输出的标准 CSV 路径。"),
        QStringLiteral("file")));
    parser.addOption(QCommandLineOption(
        {QStringLiteral("m"), QStringLiteral("max-per-meal")},
        QStringLiteral("每种餐别最多保留的食谱数量，默认 250。"),
        QStringLiteral("count"),
        QStringLiteral("250")));
    parser.process(application);

    const QString inputPath = parser.value(QStringLiteral("input"));
    const QString outputPath = parser.value(QStringLiteral("output"));
    bool countOk = false;
    const int maximumPerMeal = parser.value(
        QStringLiteral("max-per-meal")).toInt(&countOk);
    if (inputPath.isEmpty() || outputPath.isEmpty()
        || !countOk || maximumPerMeal <= 0) {
        qCritical().noquote()
            << QStringLiteral(
                   "用法：foodcom_preprocessor --input recipes.csv "
                   "--output datasets/builtin/recipes.csv "
                   "[--max-per-meal 250]");
        return 1;
    }

    CsvDataExchangeService dataExchangeService;
    FoodComPreprocessor preprocessor(dataExchangeService);
    FoodComPreprocessOptions options;
    options.maximumRecipesPerMeal = maximumPerMeal;
    const auto result = preprocessor.preprocess(
        inputPath, outputPath, options);
    if (!result.ok) {
        qCritical().noquote() << result.code << result.message;
        return 2;
    }

    qInfo().noquote()
        << QStringLiteral(
               "预处理完成：解析 %1 条，原始格式跳过 %2 条，"
               "质量筛选/去重 %3 条，最终输出 %4 条。\n输出：%5")
               .arg(result.data.parsedRows)
               .arg(result.data.malformedRows)
               .arg(result.data.filteredRows)
               .arg(result.data.selectedRows)
               .arg(result.data.outputPath);
    return 0;
}
