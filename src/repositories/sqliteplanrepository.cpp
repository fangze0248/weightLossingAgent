#include "sqliteplanrepository.h"

#include "database/modeljsoncodec.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace {

ServiceResult<WeeklyPlan> planFromQuery(const QSqlQuery& query)
{
    QString parseError;
    auto parsed = model_json_codec::weeklyPlanFromJson(
        query.value(QStringLiteral("plan_json")).toString(), &parseError);
    if (!parsed.has_value()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("PLAN_JSON_ERROR"), parseError);
    }
    return ServiceResult<WeeklyPlan>::success(*parsed);
}

} // namespace

SqlitePlanRepository::SqlitePlanRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

ServiceResult<WeeklyPlan> SqlitePlanRepository::save(const WeeklyPlan& plan)
{
    if (plan.planId.trimmed().isEmpty() || plan.userId.trimmed().isEmpty()
        || !plan.startDate.isValid()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("INVALID_PLAN"),
            QStringLiteral("Plan id, user id, and start date are required."));
    }

    WeeklyPlan storedPlan = plan;
    if (!storedPlan.generatedAt.isValid()) {
        storedPlan.generatedAt = QDateTime::currentDateTimeUtc();
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO weekly_plans "
        "(plan_id, user_id, start_date, generated_at, plan_json) "
        "VALUES (:plan_id, :user_id, :start_date, :generated_at, :plan_json) "
        "ON CONFLICT(plan_id) DO UPDATE SET user_id = excluded.user_id, "
        "start_date = excluded.start_date, generated_at = excluded.generated_at, "
        "plan_json = excluded.plan_json"));
    query.bindValue(QStringLiteral(":plan_id"), storedPlan.planId);
    query.bindValue(QStringLiteral(":user_id"), storedPlan.userId);
    query.bindValue(QStringLiteral(":start_date"), storedPlan.startDate.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":generated_at"),
                    storedPlan.generatedAt.toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":plan_json"),
                    model_json_codec::weeklyPlanToJson(storedPlan));

    if (!query.exec()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<WeeklyPlan>::success(storedPlan);
}

ServiceResult<std::optional<WeeklyPlan>> SqlitePlanRepository::findById(
    const QString& planId) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT plan_json FROM weekly_plans WHERE plan_id = :plan_id"));
    query.bindValue(QStringLiteral(":plan_id"), planId);
    if (!query.exec()) {
        return ServiceResult<std::optional<WeeklyPlan>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }
    if (!query.next()) {
        return ServiceResult<std::optional<WeeklyPlan>>::success(std::nullopt);
    }
    const auto result = planFromQuery(query);
    if (!result.ok) {
        return ServiceResult<std::optional<WeeklyPlan>>::failure(
            result.code, result.message);
    }
    return ServiceResult<std::optional<WeeklyPlan>>::success(result.data);
}

ServiceResult<QVector<WeeklyPlan>> SqlitePlanRepository::findByUserId(
    const QString& userId) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT plan_json FROM weekly_plans WHERE user_id = :user_id "
        "ORDER BY start_date DESC"));
    query.bindValue(QStringLiteral(":user_id"), userId);
    if (!query.exec()) {
        return ServiceResult<QVector<WeeklyPlan>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }

    QVector<WeeklyPlan> plans;
    while (query.next()) {
        const auto result = planFromQuery(query);
        if (!result.ok) {
            return ServiceResult<QVector<WeeklyPlan>>::failure(
                result.code, result.message);
        }
        plans.append(result.data);
    }
    return ServiceResult<QVector<WeeklyPlan>>::success(plans);
}

ServiceResult<bool> SqlitePlanRepository::remove(const QString& planId)
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "DELETE FROM weekly_plans WHERE plan_id = :plan_id"));
    query.bindValue(QStringLiteral(":plan_id"), planId);
    if (!query.exec()) {
        return ServiceResult<bool>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<bool>::success(query.numRowsAffected() > 0);
}
