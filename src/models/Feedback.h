#pragma once

#include "DomainEnums.h"

#include <QDateTime>
#include <QString>

struct Feedback {
    QString id;
    QString userId;
    RecommendationItemType itemType = RecommendationItemType::Exercise;
    QString itemId;
    FeedbackRating rating = FeedbackRating::Neutral;
    QString comment;
    QDateTime createdAt;
};
