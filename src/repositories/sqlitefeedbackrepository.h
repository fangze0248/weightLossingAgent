#pragma once

#include "interfaces/IFeedbackRepository.h"

#include <QSqlDatabase>

class SqliteFeedbackRepository final : public IFeedbackRepository
{
public:
    explicit SqliteFeedbackRepository(QSqlDatabase database);

    ServiceResult<Feedback> save(const Feedback& feedback) override;
    ServiceResult<QVector<Feedback>> findByUserId(
        const QString& userId) const override;
    ServiceResult<QVector<Feedback>> findByUserAndType(
        const QString& userId,
        RecommendationItemType itemType) const override;

private:
    QSqlDatabase database_;
};
