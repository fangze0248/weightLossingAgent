#pragma once

#include "DomainEnums.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>

struct Feedback {
    QString id;
    QString userId;
    RecommendationItemType itemType = RecommendationItemType::Exercise;
    QString itemId;

    // 旧的三档喜欢/不喜欢，保留以兼容既有调用方。
    FeedbackRating rating = FeedbackRating::Neutral;

    // 新的享受度反馈：0 表示未体验（不产生权重，不参与汇总），
    // 1～5 星依次映射到 0.6 / 0.8 / 1.0 / 1.2 / 1.4。
    int enjoymentStars = 0;

    // 结构化关键词，与 Recipe.nutritionTags（或运动类别）保持统一，
    // 避免同义词和格式差异导致无法匹配。
    QStringList keywords;

    // 关联计划与日期，允许为空。
    QString planId;
    QDate feedbackDate;

    QString comment;
    QDateTime createdAt;
};
