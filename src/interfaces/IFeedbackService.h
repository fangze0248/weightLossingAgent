#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/Feedback.h"

#include <QString>
#include <QStringList>

class IFeedbackService {
public:
    virtual ~IFeedbackService() = default;

    virtual ServiceResult<Feedback> record(const Feedback& feedback) = 0;
    virtual ServiceResult<double> recommendationWeight(
        const QString& userId,
        RecommendationItemType itemType,
        const QString& itemId) const = 0;
    virtual ServiceResult<QStringList> dislikedItemIds(
        const QString& userId,
        RecommendationItemType itemType) const = 0;
};
