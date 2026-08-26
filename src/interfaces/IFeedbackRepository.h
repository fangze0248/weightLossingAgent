#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/Feedback.h"

#include <QString>
#include <QVector>

class IFeedbackRepository {
public:
    virtual ~IFeedbackRepository() = default;

    virtual ServiceResult<Feedback> save(const Feedback& feedback) = 0;
    virtual ServiceResult<QVector<Feedback>> findByUserId(
        const QString& userId) const = 0;
    virtual ServiceResult<QVector<Feedback>> findByUserAndType(
        const QString& userId,
        RecommendationItemType itemType) const = 0;
};
