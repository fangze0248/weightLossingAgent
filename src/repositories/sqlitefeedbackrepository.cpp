#include "sqlitefeedbackrepository.h"

#include "database/modeljsoncodec.h"

#include <QDate>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <utility>

namespace {

QString itemTypeToString(RecommendationItemType value)
{
    return value == RecommendationItemType::Exercise
        ? QStringLiteral("exercise")
        : QStringLiteral("recipe");
}

RecommendationItemType itemTypeFromString(const QString& value)
{
    return value.compare(QStringLiteral("recipe"), Qt::CaseInsensitive) == 0
        ? RecommendationItemType::Recipe
        : RecommendationItemType::Exercise;
}

Feedback feedbackFromQuery(const QSqlQuery& query)
{
    Feedback feedback;
    feedback.id = query.value(QStringLiteral("id")).toString();
    feedback.userId = query.value(QStringLiteral("user_id")).toString();
    feedback.itemType = itemTypeFromString(
        query.value(QStringLiteral("item_type")).toString());
    feedback.itemId = query.value(QStringLiteral("item_id")).toString();
    feedback.rating = static_cast<FeedbackRating>(
        query.value(QStringLiteral("rating")).toInt());
    feedback.enjoymentStars =
        query.value(QStringLiteral("enjoyment_stars")).toInt();
    feedback.keywords = model_json_codec::stringListFromJson(
        query.value(QStringLiteral("keywords_json")).toString());
    feedback.planId = query.value(QStringLiteral("plan_id")).toString();
    feedback.feedbackDate = QDate::fromString(
        query.value(QStringLiteral("feedback_date")).toString(),
        Qt::ISODate);
    feedback.comment = query.value(QStringLiteral("comment")).toString();
    feedback.createdAt = QDateTime::fromString(
        query.value(QStringLiteral("created_at")).toString(), Qt::ISODateWithMs);
    if (!feedback.createdAt.isValid()) {
        feedback.createdAt = QDateTime::fromString(
            query.value(QStringLiteral("created_at")).toString(), Qt::ISODate);
    }
    return feedback;
}

ServiceResult<QVector<Feedback>> readFeedbackRows(QSqlQuery& query)
{
    if (!query.exec()) {
        return ServiceResult<QVector<Feedback>>::failure(
            QStringLiteral("DATABASE_READ_ERROR"), query.lastError().text());
    }
    QVector<Feedback> feedbackItems;
    while (query.next()) feedbackItems.append(feedbackFromQuery(query));
    return ServiceResult<QVector<Feedback>>::success(feedbackItems);
}

} // namespace

SqliteFeedbackRepository::SqliteFeedbackRepository(QSqlDatabase database)
    : database_(std::move(database))
{
}

ServiceResult<Feedback> SqliteFeedbackRepository::save(const Feedback& feedback)
{
    if (feedback.id.trimmed().isEmpty() || feedback.userId.trimmed().isEmpty()
        || feedback.itemId.trimmed().isEmpty()) {
        return ServiceResult<Feedback>::failure(
            QStringLiteral("INVALID_FEEDBACK"),
            QStringLiteral("Feedback id, user id, and item id are required."));
    }

    Feedback stored = feedback;
    if (!stored.createdAt.isValid()) {
        stored.createdAt = QDateTime::currentDateTimeUtc();
    }

    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "INSERT INTO feedback "
        "(id, user_id, item_type, item_id, rating, enjoyment_stars, "
        "keywords_json, plan_id, feedback_date, comment, created_at) "
        "VALUES (:id, :user_id, :item_type, :item_id, :rating, :enjoyment_stars, "
        ":keywords_json, :plan_id, :feedback_date, :comment, :created_at) "
        "ON CONFLICT(id) DO UPDATE SET user_id = excluded.user_id, "
        "item_type = excluded.item_type, item_id = excluded.item_id, "
        "rating = excluded.rating, enjoyment_stars = excluded.enjoyment_stars, "
        "keywords_json = excluded.keywords_json, plan_id = excluded.plan_id, "
        "feedback_date = excluded.feedback_date, comment = excluded.comment, "
        "created_at = excluded.created_at"));
    query.bindValue(QStringLiteral(":id"), stored.id);
    query.bindValue(QStringLiteral(":user_id"), stored.userId);
    query.bindValue(QStringLiteral(":item_type"), itemTypeToString(stored.itemType));
    query.bindValue(QStringLiteral(":item_id"), stored.itemId);
    query.bindValue(QStringLiteral(":rating"), static_cast<int>(stored.rating));
    query.bindValue(QStringLiteral(":enjoyment_stars"), stored.enjoymentStars);
    query.bindValue(
        QStringLiteral(":keywords_json"),
        model_json_codec::stringListToJson(stored.keywords));
    query.bindValue(
        QStringLiteral(":plan_id"),
        stored.planId.isNull() ? QStringLiteral("") : stored.planId);
    query.bindValue(
        QStringLiteral(":feedback_date"),
        stored.feedbackDate.isValid()
            ? stored.feedbackDate.toString(Qt::ISODate)
            : QStringLiteral(""));
    query.bindValue(
        QStringLiteral(":comment"),
        stored.comment.isNull() ? QStringLiteral("") : stored.comment);
    query.bindValue(QStringLiteral(":created_at"),
                    stored.createdAt.toString(Qt::ISODateWithMs));
    if (!query.exec()) {
        return ServiceResult<Feedback>::failure(
            QStringLiteral("DATABASE_WRITE_ERROR"), query.lastError().text());
    }
    return ServiceResult<Feedback>::success(stored);
}

ServiceResult<QVector<Feedback>> SqliteFeedbackRepository::findByUserId(
    const QString& userId) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT * FROM feedback WHERE user_id = :user_id ORDER BY created_at DESC"));
    query.bindValue(QStringLiteral(":user_id"), userId);
    return readFeedbackRows(query);
}

ServiceResult<QVector<Feedback>> SqliteFeedbackRepository::findByUserAndType(
    const QString& userId,
    RecommendationItemType itemType) const
{
    QSqlQuery query(database_);
    query.prepare(QStringLiteral(
        "SELECT * FROM feedback WHERE user_id = :user_id "
        "AND item_type = :item_type ORDER BY created_at DESC"));
    query.bindValue(QStringLiteral(":user_id"), userId);
    query.bindValue(QStringLiteral(":item_type"), itemTypeToString(itemType));
    return readFeedbackRows(query);
}
