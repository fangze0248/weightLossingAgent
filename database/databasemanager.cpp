#include "database/DatabaseManager.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <utility>

DatabaseManager::DatabaseManager(QString databasePath)
    : databasePath_(std::move(databasePath))
{
}

DatabaseManager::~DatabaseManager()
{
    if (!database_.isValid()) {
        return;
    }

    const QString name = database_.connectionName();

    database_.close();
    database_ = QSqlDatabase();

    QSqlDatabase::removeDatabase(name);
}

QString DatabaseManager::defaultDatabasePath()
{
    QString directory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (directory.isEmpty()) {
        directory = QDir::current().filePath(QStringLiteral("data"));
    }
    return QDir(directory).filePath(QStringLiteral("weight_agent.db"));
}

QString DatabaseManager::connectionName()
{
    return QStringLiteral("weight_agent_connection");
}

bool DatabaseManager::open(QString* errorMessage)
{
    const QFileInfo databaseFile(databasePath_);

    if (!QDir().mkpath(databaseFile.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建数据库目录");
        }
        return false;
    }

    if (QSqlDatabase::contains(connectionName())) {
        database_ = QSqlDatabase::database(connectionName());
    } else {
        database_ = QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName()
            );
    }

    database_.setDatabaseName(databasePath_);

    if (!database_.open()) {
        if (errorMessage) {
            *errorMessage = database_.lastError().text();
        }
        return false;
    }

    QSqlQuery pragmaQuery(database_);
    if (!pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        if (errorMessage) {
            *errorMessage = pragmaQuery.lastError().text();
        }
        return false;
    }

    pragmaQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL"));

    return true;
}

bool DatabaseManager::executeStatement(
    const QString& sql,
    QString* errorMessage)
{
    QSqlQuery query(database_);
    if (query.exec(sql)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = query.lastError().text();
    }
    return false;
}

bool DatabaseManager::initialize(QString* errorMessage)
{
    if (!database_.isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("The database is not open.");
        }
        return false;
    }

    const QStringList statements = {
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS users (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                gender TEXT NOT NULL CHECK(gender IN ('M', 'F')),
                age INTEGER NOT NULL CHECK(age > 0),
                height_cm REAL NOT NULL CHECK(height_cm > 0),
                weight_kg REAL NOT NULL CHECK(weight_kg > 0),
                target_weight_kg REAL NOT NULL CHECK(target_weight_kg > 0),
                activity_level INTEGER NOT NULL CHECK(activity_level BETWEEN 1 AND 5),
                goal_type TEXT NOT NULL CHECK(goal_type IN ('lose', 'maintain', 'gain')),
                weekly_goal_kg REAL NOT NULL DEFAULT 0.5,
                diet_contribution_ratio REAL NOT NULL DEFAULT 0.7,
                disliked_exercise_ids_json TEXT NOT NULL DEFAULT '[]',
                disliked_recipe_ids_json TEXT NOT NULL DEFAULT '[]'
            )
        )SQL"),

        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS exercises (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                met_value REAL NOT NULL CHECK(met_value > 0),
                category TEXT NOT NULL,
                description TEXT NOT NULL DEFAULT ''
            )
        )SQL"),

        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS recipes (
                id TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                ingredients_json TEXT NOT NULL DEFAULT '[]',
                total_calories REAL NOT NULL CHECK(total_calories >= 0),
                meal_type TEXT NOT NULL,
                nutrition_tags_json TEXT NOT NULL DEFAULT '[]'
            )
        )SQL"),

        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS weekly_plans (
                plan_id TEXT PRIMARY KEY,
                user_id TEXT NOT NULL,
                start_date TEXT NOT NULL,
                generated_at TEXT NOT NULL,
                plan_json TEXT NOT NULL,
                FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
            )
        )SQL"),

        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS feedback (
                id TEXT PRIMARY KEY,
                user_id TEXT NOT NULL,
                item_type TEXT NOT NULL CHECK(item_type IN ('exercise', 'recipe')),
                item_id TEXT NOT NULL,
                rating INTEGER NOT NULL CHECK(rating BETWEEN -1 AND 1),
                comment TEXT NOT NULL DEFAULT '',
                created_at TEXT NOT NULL,
                FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
            )
        )SQL"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_weekly_plans_user "
                       "ON weekly_plans(user_id, start_date)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_feedback_user "
                       "ON feedback(user_id, item_type)")
    };

    for (const QString& statement : statements) {
        if (!executeStatement(statement, errorMessage)) {
            return false;
        }
    }

    return true;
}

bool DatabaseManager::seedDemoData(QString* errorMessage)
{
    if (!database_.transaction()) {
        if (errorMessage) *errorMessage = database_.lastError().text();
        return false;
    }

    const QStringList statements = {
        QStringLiteral(R"SQL(
            INSERT OR IGNORE INTO users (
                id, name, gender, age, height_cm, weight_kg,
                target_weight_kg, activity_level, goal_type,
                weekly_goal_kg, diet_contribution_ratio
            ) VALUES (
                'U001', 'Demo User', 'M', 25, 175, 80,
                70, 3, 'lose', 0.5, 0.7
            )
        )SQL"),
        QStringLiteral(R"SQL(
            INSERT OR IGNORE INTO exercises
                (id, name, met_value, category, description)
            VALUES
                ('EX001', 'Brisk Walking', 4.3, 'aerobic', 'Moderate-paced walking'),
                ('EX002', 'Jogging', 7.0, 'aerobic', 'Steady moderate jogging'),
                ('EX003', 'Jump Rope', 10.0, 'aerobic', 'Moderate jump-rope training')
        )SQL"),
        QStringLiteral(R"SQL(
            INSERT OR IGNORE INTO recipes
                (id, name, ingredients_json, total_calories, meal_type, nutrition_tags_json)
            VALUES
                ('R001', 'Oat and Egg Breakfast',
                 '[{"name":"Oats","amount":50,"unit":"g"},{"name":"Egg","amount":1,"unit":"piece"}]',
                 420, 'breakfast', '["high-protein"]'),
                ('R002', 'Chicken Rice Bowl',
                 '[{"name":"Chicken breast","amount":150,"unit":"g"},{"name":"Rice","amount":150,"unit":"g"}]',
                 620, 'lunch', '["high-protein","low-fat"]'),
                ('R003', 'Steamed Fish and Vegetables',
                 '[{"name":"Fish","amount":180,"unit":"g"},{"name":"Vegetables","amount":250,"unit":"g"}]',
                 520, 'dinner', '["high-protein","low-fat"]')
        )SQL")
    };

    for (const QString& statement : statements) {
        if (!executeStatement(statement, errorMessage)) {
            database_.rollback();
            return false;
        }
    }

    if (!database_.commit()) {
        if (errorMessage) *errorMessage = database_.lastError().text();
        database_.rollback();
        return false;
    }
    return true;
}

bool DatabaseManager::isOpen() const
{
    return database_.isOpen();
}

QString DatabaseManager::databasePath() const
{
    return databasePath_;
}

QSqlDatabase DatabaseManager::database() const
{
    return database_;
}
