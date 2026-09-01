#include "database/databasemanager.h"

#include "database/modeljsoncodec.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
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

bool DatabaseManager::ensureRecipeNutritionColumns(
    QString* errorMessage)
{
    QSqlQuery query(database_);

    if (!query.exec(QStringLiteral("PRAGMA table_info(recipes)"))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    QSet<QString> existingColumns;

    while (query.next()) {
        // PRAGMA table_info 返回结果的第1列是字段名称
        existingColumns.insert(query.value(1).toString());
    }

    if (!existingColumns.contains(QStringLiteral("nutrition_json"))) {
        if (!executeStatement(
                QStringLiteral(
                    "ALTER TABLE recipes "
                    "ADD COLUMN nutrition_json "
                    "TEXT NOT NULL DEFAULT '{}'"),
                errorMessage)) {
            return false;
        }
    }

    if (!existingColumns.contains(QStringLiteral("servings"))) {
        if (!executeStatement(
                QStringLiteral(
                    "ALTER TABLE recipes "
                    "ADD COLUMN servings INTEGER NOT NULL "
                    "DEFAULT 1 CHECK(servings > 0)"),
                errorMessage)) {
            return false;
        }
    }

    const QVector<QPair<QString, QString>> searchColumns = {
        {QStringLiteral("protein_g"),
         QStringLiteral("REAL NOT NULL DEFAULT 0 CHECK(protein_g >= 0)")},
        {QStringLiteral("carbohydrate_g"),
         QStringLiteral("REAL NOT NULL DEFAULT 0 CHECK(carbohydrate_g >= 0)")},
        {QStringLiteral("fat_g"),
         QStringLiteral("REAL NOT NULL DEFAULT 0 CHECK(fat_g >= 0)")},
        {QStringLiteral("saturated_fat_g"),
         QStringLiteral("REAL NOT NULL DEFAULT 0 CHECK(saturated_fat_g >= 0)")},
        {QStringLiteral("fiber_g"),
         QStringLiteral("REAL NOT NULL DEFAULT 0 CHECK(fiber_g >= 0)")},
        {QStringLiteral("sugar_g"),
         QStringLiteral("REAL NOT NULL DEFAULT 0 CHECK(sugar_g >= 0)")},
        {QStringLiteral("sodium_mg"),
         QStringLiteral("REAL NOT NULL DEFAULT 0 CHECK(sodium_mg >= 0)")},
        {QStringLiteral("cholesterol_mg"),
         QStringLiteral("REAL NOT NULL DEFAULT 0 CHECK(cholesterol_mg >= 0)")}
    };

    bool addedSearchColumn = false;
    for (const auto& searchColumn : searchColumns) {
        if (existingColumns.contains(searchColumn.first)) continue;
        if (!executeStatement(
                QStringLiteral("ALTER TABLE recipes ADD COLUMN %1 %2")
                    .arg(searchColumn.first, searchColumn.second),
                errorMessage)) {
            return false;
        }
        addedSearchColumn = true;
    }

    // Older databases stored nutrients only as JSON. Backfill the new numeric
    // columns once so range queries can use indexes instead of parsing JSON.
    if (addedSearchColumn) {
        struct NutritionBackfill {
            QString id;
            NutritionFacts nutrition;
        };
        QVector<NutritionBackfill> rows;
        QSqlQuery select(database_);
        if (!select.exec(QStringLiteral(
                "SELECT id, total_calories, nutrition_json FROM recipes"))) {
            if (errorMessage) *errorMessage = select.lastError().text();
            return false;
        }
        while (select.next()) {
            NutritionFacts nutrition =
                model_json_codec::nutritionFactsFromJson(
                    select.value(2).toString());
            if (nutrition.caloriesKcal <= 0.0) {
                nutrition.caloriesKcal = select.value(1).toDouble();
            }
            rows.append({select.value(0).toString(), nutrition});
        }

        QSqlQuery update(database_);
        update.prepare(QStringLiteral(
            "UPDATE recipes SET protein_g = :protein, "
            "carbohydrate_g = :carbohydrate, fat_g = :fat, "
            "saturated_fat_g = :saturated_fat, fiber_g = :fiber, "
            "sugar_g = :sugar, sodium_mg = :sodium, "
            "cholesterol_mg = :cholesterol WHERE id = :id"));
        for (const NutritionBackfill& row : rows) {
            update.bindValue(QStringLiteral(":id"), row.id);
            update.bindValue(QStringLiteral(":protein"), row.nutrition.proteinG);
            update.bindValue(
                QStringLiteral(":carbohydrate"),
                row.nutrition.carbohydrateG);
            update.bindValue(QStringLiteral(":fat"), row.nutrition.fatG);
            update.bindValue(
                QStringLiteral(":saturated_fat"),
                row.nutrition.saturatedFatG);
            update.bindValue(QStringLiteral(":fiber"), row.nutrition.fiberG);
            update.bindValue(QStringLiteral(":sugar"), row.nutrition.sugarG);
            update.bindValue(QStringLiteral(":sodium"), row.nutrition.sodiumMg);
            update.bindValue(
                QStringLiteral(":cholesterol"),
                row.nutrition.cholesterolMg);
            if (!update.exec()) {
                if (errorMessage) *errorMessage = update.lastError().text();
                return false;
            }
        }
    }

    return true;
}

bool DatabaseManager::ensureUserAverageDailyStepsColumn(
    QString* errorMessage)
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("PRAGMA table_info(users)"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }

    bool columnExists = false;
    while (query.next()) {
        if (query.value(1).toString()
            == QStringLiteral("average_daily_steps")) {
            columnExists = true;
            break;
        }
    }
    if (columnExists) return true;

    // Existing users receive a conservative baseline and can replace it in
    // the profile UI with their actual past-seven-day average.
    return executeStatement(
        QStringLiteral(
            "ALTER TABLE users ADD COLUMN average_daily_steps "
            "INTEGER NOT NULL DEFAULT 4000 "
            "CHECK(average_daily_steps BETWEEN 0 AND 50000)"),
        errorMessage);
}

bool DatabaseManager::ensureUserExerciseGoalColumn(QString* errorMessage)
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("PRAGMA table_info(users)"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }

    bool columnExists = false;
    while (query.next()) {
        if (query.value(1).toString() == QStringLiteral("exercise_goal")) {
            columnExists = true;
            break;
        }
    }
    if (columnExists) return true;

    // Existing users default to the lightest goal so old callers keep working.
    return executeStatement(
        QStringLiteral(
            "ALTER TABLE users ADD COLUMN exercise_goal TEXT NOT NULL "
            "DEFAULT 'light_health' "
            "CHECK(exercise_goal IN ('light_health', 'build_fitness', 'muscle_gain'))"),
        errorMessage);
}

bool DatabaseManager::ensureFeedbackColumns(QString* errorMessage)
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("PRAGMA table_info(feedback)"))) {
        if (errorMessage) *errorMessage = query.lastError().text();
        return false;
    }

    QSet<QString> existingColumns;
    while (query.next()) {
        existingColumns.insert(query.value(1).toString());
    }

    const QVector<QPair<QString, QString>> columns = {
        {QStringLiteral("enjoyment_stars"),
         QStringLiteral("INTEGER NOT NULL DEFAULT 0 "
                        "CHECK(enjoyment_stars BETWEEN 0 AND 5)")},
        {QStringLiteral("keywords_json"),
         QStringLiteral("TEXT NOT NULL DEFAULT '[]'")},
        {QStringLiteral("plan_id"),
         QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("feedback_date"),
         QStringLiteral("TEXT NOT NULL DEFAULT ''")}
    };

    for (const auto& column : columns) {
        if (existingColumns.contains(column.first)) continue;
        if (!executeStatement(
                QStringLiteral("ALTER TABLE feedback ADD COLUMN %1 %2")
                    .arg(column.first, column.second),
                errorMessage)) {
            return false;
        }
    }
    return true;
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
                average_daily_steps INTEGER NOT NULL DEFAULT 4000
                CHECK(average_daily_steps BETWEEN 0 AND 50000),
                activity_level INTEGER NOT NULL CHECK(activity_level BETWEEN 1 AND 5),
                goal_type TEXT NOT NULL CHECK(goal_type IN ('lose', 'maintain', 'gain')),
                exercise_goal TEXT NOT NULL DEFAULT 'light_health'
                CHECK(exercise_goal IN ('light_health', 'build_fitness', 'muscle_gain')),
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
                nutrition_json TEXT NOT NULL DEFAULT '{}',
                servings INTEGER NOT NULL DEFAULT 1
                CHECK(servings > 0),
                protein_g REAL NOT NULL DEFAULT 0 CHECK(protein_g >= 0),
                carbohydrate_g REAL NOT NULL DEFAULT 0
                CHECK(carbohydrate_g >= 0),
                fat_g REAL NOT NULL DEFAULT 0 CHECK(fat_g >= 0),
                saturated_fat_g REAL NOT NULL DEFAULT 0
                CHECK(saturated_fat_g >= 0),
                fiber_g REAL NOT NULL DEFAULT 0 CHECK(fiber_g >= 0),
                sugar_g REAL NOT NULL DEFAULT 0 CHECK(sugar_g >= 0),
                sodium_mg REAL NOT NULL DEFAULT 0 CHECK(sodium_mg >= 0),
                cholesterol_mg REAL NOT NULL DEFAULT 0
                CHECK(cholesterol_mg >= 0),
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
                enjoyment_stars INTEGER NOT NULL DEFAULT 0
                CHECK(enjoyment_stars BETWEEN 0 AND 5),
                keywords_json TEXT NOT NULL DEFAULT '[]',
                plan_id TEXT NOT NULL DEFAULT '',
                feedback_date TEXT NOT NULL DEFAULT '',
                comment TEXT NOT NULL DEFAULT '',
                created_at TEXT NOT NULL,
                FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
            )
        )SQL"),
        QStringLiteral(R"SQL(
            CREATE TABLE IF NOT EXISTS dataset_imports (
                dataset_key TEXT PRIMARY KEY,
                content_hash TEXT NOT NULL,
                imported_at TEXT NOT NULL,
                imported_rows INTEGER NOT NULL CHECK(imported_rows >= 0)
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
    if (!ensureRecipeNutritionColumns(errorMessage)) {
        return false;
    }
    if (!ensureUserAverageDailyStepsColumn(errorMessage)) {
        return false;
    }
    if (!ensureUserExerciseGoalColumn(errorMessage)) {
        return false;
    }
    if (!ensureFeedbackColumns(errorMessage)) {
        return false;
    }
    const QStringList searchIndexes = {
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_recipes_meal_calories "
            "ON recipes(meal_type, total_calories)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_recipes_nutrition "
            "ON recipes(protein_g, fiber_g, sodium_mg)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_exercises_category_met "
            "ON exercises(category, met_value)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_exercises_met "
            "ON exercises(met_value)")
    };
    for (const QString& statement : searchIndexes) {
        if (!executeStatement(statement, errorMessage)) return false;
    }
    return executeStatement(QStringLiteral("PRAGMA user_version = 5"),
                            errorMessage);
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
                target_weight_kg, average_daily_steps, activity_level, goal_type,
                weekly_goal_kg, diet_contribution_ratio
            ) VALUES (
                'U001', 'Demo User', 'M', 25, 175, 80,
                70, 4000, 3, 'lose', 0.5, 0.7
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
                 520, 'dinner', '["high-protein","low-fat"]'),
                ('R004', 'Yogurt Banana Breakfast',
                 '[{"name":"Yogurt","amount":200,"unit":"g"},{"name":"Banana","amount":1,"unit":"piece"}]',
                 300, 'breakfast', '["balanced"]'),
                ('R005', 'Beef Quinoa Bowl',
                 '[{"name":"Lean beef","amount":120,"unit":"g"},{"name":"Quinoa","amount":100,"unit":"g"}]',
                 450, 'lunch', '["high-protein"]'),
                ('R006', 'Tofu Vegetable Dinner',
                 '[{"name":"Tofu","amount":180,"unit":"g"},{"name":"Vegetables","amount":250,"unit":"g"}]',
                 400, 'dinner', '["plant-protein","high-fiber"]')
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
