#pragma once

#include <QHash>
#include <QString>

#include <optional>

// 由反馈统计层生成、由推荐核心消费的纯数据对象。
// 推荐核心不直接依赖界面、数据库或反馈仓储。
struct RecommendationPreference {
    // 项目 ID -> 历史权重；未出现的项目按 1.0 处理。
    QHash<QString, double> itemWeights;
    // 规范化关键词 -> 用户偏好强度。正值偏好，负值排斥。
    QHash<QString, double> keywordWeights;

    bool hasSignals() const noexcept
    {
        return !itemWeights.isEmpty() || !keywordWeights.isEmpty();
    }
};

// 0 表示未体验，不产生权重；1～5 星依次映射到
// 0.6、0.8、1.0、1.2、1.4。非法星级同样返回空值。
inline std::optional<double> feedbackWeightFromStars(int stars) noexcept
{
    if (stars < 1 || stars > 5) {
        return std::nullopt;
    }
    return 1.0 + 0.2 * static_cast<double>(stars - 3);
}
