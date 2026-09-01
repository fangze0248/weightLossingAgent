#include "services/FeedbackService.h"

#include "interfaces/IFeedbackRepository.h"

#include <QHash>
#include <QSet>

#include <utility>

namespace {

QString normalizedKeyword(const QString& value)
{
    // 与推荐核心的 normalizedKeyword 保持一致，避免大小写/空白导致失配。
    return value.trimmed().toLower();
}

} // namespace

FeedbackService::FeedbackService(IFeedbackRepository& repository)
    : repository_(repository)
{
}

ServiceResult<Feedback> FeedbackService::record(const Feedback& feedback)
{
    if (feedback.id.trimmed().isEmpty() || feedback.userId.trimmed().isEmpty()
        || feedback.itemId.trimmed().isEmpty()) {
        return ServiceResult<Feedback>::failure(
            QStringLiteral("INVALID_FEEDBACK"),
            QStringLiteral("Feedback id, user id, and item id are required."));
    }

    if (feedback.enjoymentStars < 0 || feedback.enjoymentStars > 5) {
        return ServiceResult<Feedback>::failure(
            QStringLiteral("INVALID_FEEDBACK"),
            QStringLiteral("Enjoyment stars must be between 0 and 5."));
    }

    const bool hasStars = feedback.enjoymentStars >= 1;
    const bool hasRating = feedback.rating != FeedbackRating::Neutral;
    if (!hasStars && !hasRating) {
        // 未体验（既无星级、也无喜欢/不喜欢）不写入数据库，视为无反馈。
        return ServiceResult<Feedback>::success(
            feedback,
            QStringLiteral("未体验不产生反馈，已忽略。"));
    }

    return repository_.save(feedback);
}

ServiceResult<double> FeedbackService::recommendationWeight(
    const QString& userId,
    RecommendationItemType itemType,
    const QString& itemId) const
{
    const auto preferenceResult = buildPreference(userId, itemType);
    if (!preferenceResult.ok) {
        return ServiceResult<double>::failure(
            preferenceResult.code,
            preferenceResult.message,
            preferenceResult.warnings);
    }
    // 无历史星级时按中性权重 1.0 处理。
    return ServiceResult<double>::success(
        preferenceResult.data.itemWeights.value(itemId, 1.0));
}

ServiceResult<QStringList> FeedbackService::dislikedItemIds(
    const QString& userId,
    RecommendationItemType itemType) const
{
    const auto feedbackResult = repository_.findByUserAndType(userId, itemType);
    if (!feedbackResult.ok) {
        return ServiceResult<QStringList>::failure(
            feedbackResult.code,
            feedbackResult.message,
            feedbackResult.warnings);
    }

    QSet<QString> disliked;
    for (const Feedback& feedback : feedbackResult.data) {
        if (feedback.rating == FeedbackRating::Dislike) {
            disliked.insert(feedback.itemId.trimmed());
        }
    }

    QStringList ids = disliked.values();
    ids.sort();
    return ServiceResult<QStringList>::success(ids);
}

ServiceResult<RecommendationPreference> FeedbackService::buildPreference(
    const QString& userId,
    RecommendationItemType itemType) const
{
    const auto feedbackResult = repository_.findByUserAndType(userId, itemType);
    if (!feedbackResult.ok) {
        return ServiceResult<RecommendationPreference>::failure(
            feedbackResult.code,
            feedbackResult.message,
            feedbackResult.warnings);
    }

    // itemWeights：对每个项目取历史星级权重（0.6~1.4）的平均值。
    QHash<QString, double> starSum;
    QHash<QString, int> starCount;
    // keywordWeights：每个关键词累加“偏离中性”的偏好强度，正值偏好、负值排斥。
    QHash<QString, double> keywordWeights;

    for (const Feedback& feedback : feedbackResult.data) {
        if (feedback.enjoymentStars < 1 || feedback.enjoymentStars > 5) {
            continue; // 未体验或非法星级不参与汇总。
        }
        const auto weight = feedbackWeightFromStars(feedback.enjoymentStars);
        if (!weight.has_value()) {
            continue;
        }

        const QString itemId = feedback.itemId.trimmed();
        if (!itemId.isEmpty()) {
            starSum[itemId] += *weight;
            starCount[itemId] += 1;
        }

        const double delta = *weight - 1.0;
        for (const QString& keyword : feedback.keywords) {
            const QString normalized = normalizedKeyword(keyword);
            if (normalized.isEmpty()) {
                continue;
            }
            keywordWeights[normalized] += delta;
        }
    }

    RecommendationPreference preference;
    for (auto it = starSum.cbegin(); it != starSum.cend(); ++it) {
        const int count = starCount.value(it.key(), 1);
        if (count > 0) {
            preference.itemWeights.insert(it.key(), it.value() / count);
        }
    }
    preference.keywordWeights = std::move(keywordWeights);

    return ServiceResult<RecommendationPreference>::success(preference);
}
