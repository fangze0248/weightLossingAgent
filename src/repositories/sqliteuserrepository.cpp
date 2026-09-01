#include "sqliteuserrepository.h"

#include "database/modeljsoncodec.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace {

UserProfile userFromQuery(const QSqlQuery& query)
{
    UserProfile user;
    user.id = query.value(QStringLiteral("id")).toString();
    user.name = query.value(QStringLiteral("name")).toString();
    user.gender = genderFromStorageString(
                      query.value(QStringLiteral("gender")).toString())
                      .value_or(Gender::Male);
    user.age = query.value(QStringLiteral("age")).toInt();
    user.heightCm = query.value(QStringLiteral("height_cm")).toDouble();
    user.weightKg = query.value(QStringLiteral("weight_kg")).toDouble();
    user.targetWeightKg = query.value(QStringLiteral("target_weight_kg")).toDouble();
    user.averageDailySteps =
        query.value(QStringLiteral("average_daily_steps")).toInt();
    user.activityLevel = query.value(QStringLiteral("activity_level")).toInt();
    user.goalType = goalTypeFromStorageString(
                        query.value(QStringLiteral("goal_type")).toString())
                        .value_or(GoalType::Lose);
    user.weeklyGoalKg = query.value(QStringLiteral("weekly_goal_kg")).toDouble();
    user.dietContributionRatio =
        query.value(QStringLiteral("diet_contribution_ratio")).toDouble();
    user.dislikedExerciseIds = model_json_codec::stringListFromJson(
        query.value(QStringLiteral("disliked_exercise_ids_json")).toString());
    user.dislikedRecipeIds = model_json_codec::stringListFromJson(
        query.value(QStringLiteral("disliked_recipe_ids_json")).toString());
    return user;
}

ServiceResult<UserProfile> validateUser(const UserProfile& user)
{
    if (user.id.trimmed().isEmpty() || user.name.trimmed().isEmpty()) {
        return ServiceResult<UserProfile>::failure(
            QStringLiteral("INVALID_USER"),
            QStringLiteral("User id and name are required."));
    }
    if (user.age <= 0 || user.heightCm <= 0.0 || user.weightKg <= 0.0
        || user.targetWeightKg <= 0.0 || user.averageDailySteps < 0
        || user.averageDailySteps > 50000 || user.activityLevel < 1
        || user.activityLevel > 5) {
        return ServiceResult<UserProfile>::failure(
            QStringLiteral("INVALID_USER"),
            QStringLiteral("User health values are outside the valid range."));
    }
    if (user.dietContributionRatio < 0.0 || user.dietContributionRatio > 1.0) {
        return ServiceResult<UserProfile>::failure(
            QStringLiteral("INVALID_USER"),
            QStringLiteral("Diet contribution ratio must be between 0 and 1."));
    }
    return ServiceResult<UserProfile>::success(user);
}

void bindUser(QSqlQuery& query, const UserProfile& user)
{
    query.bindValue(QStringLiteral(":id"), user.id.trimmed());
    query.bindValue(QStringLiteral(":name"), user.name.trimmed());
    query.bindValue(QStringLiteral(":gender"), toStorageString(user.gender));
    query.bindValue(QStringLiteral(":age"), user.age);
    query.bindValue(QStringLiteral(":height_cm"), user.heightCm);
    query.bindValue(QStringLiteral(":weight_kg"), user.weightKg);
    query.bindValue(QStringLiteral(":target_weight_kg"), user.targetWeightKg);
    query.bindValue(QStringLiteral(":average_daily_steps"),
                    user.averageDailySteps);
    query.bindValue(QStringLiteral(":activity_level"), user.activityLevel);
    query.bindValue(QStringLiteral(":goal_type"), toStorageString(user.goalType));
    query.bindValue(QStringLiteral(":weekly_goal_kg"), user.weeklyGoalKg);
    query.bindValue(QStringLiteral(":diet_ratio"), user.dietContributionRatio);
    query.bindValue(QStringLiteral(":disliked_exercises"),
                    model_json_codec::stringListToJson(user.dislikedExerciseIds));
    query.bindValue(QStringLiteral(":disliked_recipes"),
                    model_json_codec::stringListToJson(user.dislikedRecipeIds));
}

} // namespace

SqliteUserRepository::SqliteUserRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

ServiceResult<QVector<UserProfile>> SqliteUserRepository::findAll() const
{
    QSqlQuery query(database_);
    if (!query.exec(QStringLiteral("SELECT * FROM users ORDER BY name"))) {
        return ServiceResult<QVector<UserProfile>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }

    QVector<UserProfile> users;
    while (query.next()) users.append(userFromQuery(query));
    return ServiceResult<QVector<UserProfile>>::success(users);
}

ServiceResult<std::optional<UserProfile>> SqliteUserRepository::findById(
    const QString& id) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("SELECT * FROM users WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        return ServiceResult<std::optional<UserProfile>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }
    if (!query.next()) {
        return ServiceResult<std::optional<UserProfile>>::success(std::nullopt);
    }
    return ServiceResult<std::optional<UserProfile>>::success(userFromQuery(query));
}

ServiceResult<UserProfile> SqliteUserRepository::add(const UserProfile& user)
{
    const auto validation = validateUser(user);
    if (!validation.ok) return validation;

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO users (id, name, gender, age, height_cm, weight_kg, "
        "target_weight_kg, average_daily_steps, activity_level, goal_type, "
        "weekly_goal_kg, "
        "diet_contribution_ratio, disliked_exercise_ids_json, "
        "disliked_recipe_ids_json) VALUES (:id, :name, :gender, :age, "
        ":height_cm, :weight_kg, :target_weight_kg, :average_daily_steps, "
        ":activity_level, :goal_type, :weekly_goal_kg, :diet_ratio, "
        ":disliked_exercises, "
        ":disliked_recipes)"));
    bindUser(query, user);
    if (!query.exec()) {
        return ServiceResult<UserProfile>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<UserProfile>::success(user);
}

ServiceResult<UserProfile> SqliteUserRepository::update(const UserProfile& user)
{
    const auto validation = validateUser(user);
    if (!validation.ok) return validation;

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "UPDATE users SET name = :name, gender = :gender, age = :age, "
        "height_cm = :height_cm, weight_kg = :weight_kg, "
        "target_weight_kg = :target_weight_kg, "
        "average_daily_steps = :average_daily_steps, "
        "activity_level = :activity_level, "
        "goal_type = :goal_type, weekly_goal_kg = :weekly_goal_kg, "
        "diet_contribution_ratio = :diet_ratio, "
        "disliked_exercise_ids_json = :disliked_exercises, "
        "disliked_recipe_ids_json = :disliked_recipes WHERE id = :id"));
    bindUser(query, user);
    if (!query.exec()) {
        return ServiceResult<UserProfile>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    if (query.numRowsAffected() == 0) {
        return ServiceResult<UserProfile>::failure(
            QStringLiteral("USER_NOT_FOUND"), QStringLiteral("User not found."));
    }
    return ServiceResult<UserProfile>::success(user);
}

ServiceResult<bool> SqliteUserRepository::remove(const QString& id)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral("DELETE FROM users WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec()) {
        return ServiceResult<bool>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<bool>::success(query.numRowsAffected() > 0);
}
