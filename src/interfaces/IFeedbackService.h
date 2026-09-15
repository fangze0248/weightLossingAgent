#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/Feedback.h"
#include "../models/RecommendationPreference.h"

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
    // 汇总某个用户在某类型下的历史享受度反馈：itemWeights 按项目 ID、
    // keywordWeights 按结构化关键词，供推荐器在生成新计划前消费。
    virtual ServiceResult<RecommendationPreference> buildPreference(
        const QString& userId,
        RecommendationItemType itemType) const = 0;
};
