#include "sqliteexerciseRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace {

ExerciseCategory exerciseCategoryFromStorageString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("aerobic")) return ExerciseCategory::Aerobic;
    if (normalized == QStringLiteral("strength")) return ExerciseCategory::Strength;
    if (normalized == QStringLiteral("flexibility")) return ExerciseCategory::Flexibility;
    if (normalized == QStringLiteral("balance")) return ExerciseCategory::Balance;
    return ExerciseCategory::Other;
}

Exercise exerciseFromQuery(const QSqlQuery& query)
{
    Exercise exercise;
    exercise.id = query.value(QStringLiteral("id")).toString();
    exercise.name = query.value(QStringLiteral("name")).toString();
    exercise.metValue = query.value(QStringLiteral("met_value")).toDouble();
    exercise.category = exerciseCategoryFromStorageString(
        query.value(QStringLiteral("category")).toString());
    exercise.description = query.value(QStringLiteral("description")).toString();
    return exercise;
}

ServiceResult<Exercise> validateExercise(const Exercise& exercise)
{
    if (exercise.id.trimmed().isEmpty() || exercise.name.trimmed().isEmpty()) {
        return ServiceResult<Exercise>::failure(
            QStringLiteral("INVALID_EXERCISE"),
            QStringLiteral("Exercise id and name are required."));
    }
    if (exercise.metValue <= 0.0) {
        return ServiceResult<Exercise>::failure(
            QStringLiteral("INVALID_EXERCISE"),
            QStringLiteral("MET value must be greater than zero."));
    }
    return ServiceResult<Exercise>::success(exercise);
}

} // namespace

SqliteExerciseRepository::SqliteExerciseRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

ServiceResult<QVector<Exercise>> SqliteExerciseRepository::findAll(
    const ExerciseFilter& filter) const
{
    QString sql = QStringLiteral(
        "SELECT id, name, met_value, category, description "
        "FROM exercises WHERE 1 = 1");

    if (!filter.keyword.trimmed().isEmpty()) {
        sql += QStringLiteral(
            " AND (name LIKE :keyword OR description LIKE :keyword)");
    }
    if (filter.category.has_value()) {
        sql += QStringLiteral(" AND category = :category");
    }
    if (filter.minimumMet.has_value()) {
        sql += QStringLiteral(" AND met_value >= :minimum_met");
    }
    if (filter.maximumMet.has_value()) {
        sql += QStringLiteral(" AND met_value <= :maximum_met");
    }
    sql += QStringLiteral(" ORDER BY name");

    QSqlQuery query(database_);
    query.prepare(sql);
    if (!filter.keyword.trimmed().isEmpty()) {
        query.bindValue(QStringLiteral(":keyword"),
                        QStringLiteral("%") + filter.keyword.trimmed()
                            + QStringLiteral("%"));
    }
    if (filter.category.has_value()) {
        query.bindValue(QStringLiteral(":category"),
                        toStorageString(*filter.category));
    }
    if (filter.minimumMet.has_value()) {
        query.bindValue(QStringLiteral(":minimum_met"), *filter.minimumMet);
    }
    if (filter.maximumMet.has_value()) {
        query.bindValue(QStringLiteral(":maximum_met"), *filter.maximumMet);
    }

    if (!query.exec()) {
        return ServiceResult<QVector<Exercise>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }

    QVector<Exercise> exercises;
    while (query.next()) {
        exercises.append(exerciseFromQuery(query));
    }
    return ServiceResult<QVector<Exercise>>::success(exercises);
}

ServiceResult<std::optional<Exercise>> SqliteExerciseRepository::findById(
    const QString& id) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT id, name, met_value, category, description "
        "FROM exercises WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);

    if (!query.exec()) {
        return ServiceResult<std::optional<Exercise>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }
    if (!query.next()) {
        return ServiceResult<std::optional<Exercise>>::success(std::nullopt);
    }
    return ServiceResult<std::optional<Exercise>>::success(
        exerciseFromQuery(query));
}

ServiceResult<Exercise> SqliteExerciseRepository::add(const Exercise& exercise)
{
    const auto validation = validateExercise(exercise);
    if (!validation.ok) return validation;

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO exercises (id, name, met_value, category, description) "
        "VALUES (:id, :name, :met, :category, :description)"));
    query.bindValue(QStringLiteral(":id"), exercise.id.trimmed());
    query.bindValue(QStringLiteral(":name"), exercise.name.trimmed());
    query.bindValue(QStringLiteral(":met"), exercise.metValue);
    query.bindValue(QStringLiteral(":category"), toStorageString(exercise.category));
    query.bindValue(
        QStringLiteral(":description"),
        exercise.description.isNull() ? QStringLiteral("") : exercise.description);

    if (!query.exec()) {
        return ServiceResult<Exercise>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<Exercise>::success(exercise);
}

ServiceResult<Exercise> SqliteExerciseRepository::update(const Exercise& exercise)
{
    const auto validation = validateExercise(exercise);
    if (!validation.ok) return validation;

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE exercises SET name = :name, met_value = :met, "
        "category = :category, description = :description WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), exercise.id.trimmed());
    query.bindValue(QStringLiteral(":name"), exercise.name.trimmed());
    query.bindValue(QStringLiteral(":met"), exercise.metValue);
    query.bindValue(QStringLiteral(":category"), toStorageString(exercise.category));
    query.bindValue(
        QStringLiteral(":description"),
        exercise.description.isNull() ? QStringLiteral("") : exercise.description);

    if (!query.exec()) {
        return ServiceResult<Exercise>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        return ServiceResult<Exercise>::failure(
            QStringLiteral("EXERCISE_NOT_FOUND"),
            QStringLiteral("Exercise not found."));
    }
    return ServiceResult<Exercise>::success(exercise);
}

ServiceResult<bool> SqliteExerciseRepository::remove(const QString& id)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM exercises WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        return ServiceResult<bool>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<bool>::success(query.numRowsAffected() > 0);
}
