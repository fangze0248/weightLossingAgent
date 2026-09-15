#pragma once

#include "interfaces/IFeedbackService.h"

class IFeedbackRepository;

/**
 * 反馈服务：负责保存享受度反馈，并把某个用户的历史星级汇总成
 * RecommendationPreference（itemWeights + keywordWeights），供推荐核心消费。
 */
class FeedbackService final : public IFeedbackService
{
public:
    explicit FeedbackService(IFeedbackRepository& repository);

    ServiceResult<Feedback> record(const Feedback& feedback) override;
    ServiceResult<double> recommendationWeight(
        const QString& userId,
        RecommendationItemType itemType,
        const QString& itemId) const override;
    ServiceResult<QStringList> dislikedItemIds(
        const QString& userId,
        RecommendationItemType itemType) const override;
    ServiceResult<RecommendationPreference> buildPreference(
        const QString& userId,
        RecommendationItemType itemType) const override;

private:
    IFeedbackRepository& repository_;
};
