#include "application/builtindatasetinitializer.h"
#include "application/csvdataexchangeservice.h"
#include "database/databasemanager.h"
#include "repositories/sqliteexerciserepository.h"
#include "repositories/sqlitereciperepository.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

namespace {

void printSummary(const QString& label,
                  const DatasetInitializationSummary& summary)
{
    if (!summary.imported) {
        qInfo().noquote()
            << QStringLiteral("%1：文件未变化，已跳过。").arg(label);
        return;
    }
    qInfo().noquote()
        << QStringLiteral("%1：解析 %2 条，写入 %3 条，跳过 %4 条。")
               .arg(label)
               .arg(summary.parsedRows)
               .arg(summary.storedRows)
               .arg(summary.skippedRows);
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SummerSchool"));
    QCoreApplication::setApplicationName(QStringLiteral("WeightLossingAgent"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("将预处理后的食谱/运动 CSV 批量写入 SQLite。"));
    parser.addHelpOption();
    parser.addOption(QCommandLineOption(
        {QStringLiteral("d"), QStringLiteral("database")},
        QStringLiteral("SQLite 文件路径；省略时使用程序默认数据库。"),
        QStringLiteral("file")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("recipes"),
        QStringLiteral("预处理后的食谱 CSV 路径。"),
        QStringLiteral("file")));
    parser.addOption(QCommandLineOption(
        QStringLiteral("exercises"),
        QStringLiteral("预处理后的运动 CSV 路径。"),
        QStringLiteral("file")));
    parser.process(application);

    const QString recipePath = parser.value(QStringLiteral("recipes"));
    const QString exercisePath = parser.value(QStringLiteral("exercises"));
    if (recipePath.isEmpty() && exercisePath.isEmpty()) {
        qCritical().noquote()
            << QStringLiteral(
                   "至少需要提供 --recipes 或 --exercises 之一。");
        return 1;
    }

    QString databasePath = parser.value(QStringLiteral("database"));
    if (databasePath.isEmpty()) {
        databasePath = DatabaseManager::defaultDatabasePath();
    }

    DatabaseManager manager(databasePath);
    QString error;
    if (!manager.open(&error) || !manager.initialize(&error)) {
        qCritical().noquote() << QStringLiteral("数据库初始化失败：") << error;
        return 2;
    }

    SqliteExerciseRepository exerciseRepository(manager.database());
    SqliteRecipeRepository recipeRepository(manager.database());
    CsvDataExchangeService dataExchangeService;
    BuiltinDatasetInitializer initializer(
        manager.database(),
        exerciseRepository,
        recipeRepository,
        dataExchangeService);

    if (!exercisePath.isEmpty()) {
        const auto result = initializer.importExercisesIfChanged(
            QStringLiteral("external_exercises"), exercisePath);
        if (!result.ok) {
            qCritical().noquote()
                << QStringLiteral("运动导入失败：")
                << result.code << result.message;
            return 3;
        }
        printSummary(QStringLiteral("运动"), result.data);
    }

    if (!recipePath.isEmpty()) {
        const auto result = initializer.importRecipesIfChanged(
            QStringLiteral("external_recipes"), recipePath);
        if (!result.ok) {
            qCritical().noquote()
                << QStringLiteral("食谱导入失败：")
                << result.code << result.message;
            return 4;
        }
        printSummary(QStringLiteral("食谱"), result.data);
    }

    qInfo().noquote()
        << QStringLiteral("数据库导入完成：%1").arg(databasePath);
    return 0;
}
