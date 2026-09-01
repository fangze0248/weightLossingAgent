#include "database/databasemanager.h"
#include "repositories/sqlitefeedbackrepository.h"
#include "services/FeedbackService.h"

#include <QCoreApplication>
#include <QTemporaryDir>

#include <cmath>

namespace {

bool near(double left, double right)
{
    return std::abs(left - right) < 1e-9;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) return 1;

    DatabaseManager manager(
        temporaryDirectory.filePath(QStringLiteral("feedback_test.db")));
    QString error;
    if (!manager.open(&error)) return 2;
    if (!manager.initialize(&error)) return 3;
    if (!manager.seedDemoData(&error)) return 4;

    SqliteFeedbackRepository repository(manager.database());
    FeedbackService service(repository);

    // 1) 享受度反馈 + 结构化关键词
    Feedback first;
    first.id = QStringLiteral("FB1");
    first.userId = QStringLiteral("U001");
    first.itemType = RecommendationItemType::Recipe;
    first.itemId = QStringLiteral("R001");
    first.enjoymentStars = 5;
    first.keywords = {QStringLiteral("高蛋白"), QStringLiteral("清淡")};
    if (!service.record(first).ok) return 10;

    Feedback second;
    second.id = QStringLiteral("FB2");
    second.userId = QStringLiteral("U001");
    second.itemType = RecommendationItemType::Recipe;
    second.itemId = QStringLiteral("R002");
    second.enjoymentStars = 4;
    second.keywords = {QStringLiteral("高蛋白")};
    if (!service.record(second).ok) return 11;

    // 2) 同一项目再次打分，itemWeight 应取平均值 (1.4 + 1.0) / 2 = 1.2
    Feedback third;
    third.id = QStringLiteral("FB3");
    third.userId = QStringLiteral("U001");
    third.itemType = RecommendationItemType::Recipe;
    third.itemId = QStringLiteral("R001");
    third.enjoymentStars = 3;
    if (!service.record(third).ok) return 12;

    // 3) 未体验（无星级、无喜欢/不喜欢）不应写入数据库
    Feedback unexperienced;
    unexperienced.id = QStringLiteral("FB4");
    unexperienced.userId = QStringLiteral("U001");
    unexperienced.itemType = RecommendationItemType::Recipe;
    unexperienced.itemId = QStringLiteral("R003");
    unexperienced.enjoymentStars = 0;
    if (!service.record(unexperienced).ok) return 13;

    // 4) 旧 rating 型 dislike 用于 dislikedItemIds
    Feedback dislike;
    dislike.id = QStringLiteral("FB5");
    dislike.userId = QStringLiteral("U001");
    dislike.itemType = RecommendationItemType::Exercise;
    dislike.itemId = QStringLiteral("EX001");
    dislike.rating = FeedbackRating::Dislike;
    if (!service.record(dislike).ok) return 14;

    // 汇总食谱偏好
    const auto preference =
        service.buildPreference(QStringLiteral("U001"),
                                RecommendationItemType::Recipe);
    if (!preference.ok) return 15;

    // R001 平均 1.2，R002 单次 1.2
    if (!near(preference.data.itemWeights.value(QStringLiteral("R001"), 0.0),
              1.2)) return 16;
    if (!near(preference.data.itemWeights.value(QStringLiteral("R002"), 0.0),
              1.2)) return 17;
    // 未体验的 R003 不产生权重
    if (preference.data.itemWeights.contains(QStringLiteral("R003"))) return 18;

    // 关键词净偏好：高蛋白 = (1.4-1.0)+(1.2-1.0) = 0.6；清淡 = 0.4
    if (!near(preference.data.keywordWeights.value(QStringLiteral("高蛋白"), 0.0),
              0.6)) return 19;
    if (!near(preference.data.keywordWeights.value(QStringLiteral("清淡"), 0.0),
              0.4)) return 20;

    // recommendationWeight：R001 -> 1.2，无反馈项目 -> 1.0
    const auto weight =
        service.recommendationWeight(QStringLiteral("U001"),
                                     RecommendationItemType::Recipe,
                                     QStringLiteral("R001"));
    if (!weight.ok || !near(weight.data, 1.2)) return 21;
    const auto missingWeight =
        service.recommendationWeight(QStringLiteral("U001"),
                                     RecommendationItemType::Recipe,
                                     QStringLiteral("R999"));
    if (!missingWeight.ok || !near(missingWeight.data, 1.0)) return 22;

    // dislikedItemIds：仅 EX001
    const auto disliked =
        service.dislikedItemIds(QStringLiteral("U001"),
                                RecommendationItemType::Exercise);
    if (!disliked.ok || disliked.data.size() != 1
        || disliked.data.first() != QStringLiteral("EX001")) return 23;

    // 未体验不落库：食谱反馈应只有 FB1/FB2/FB3 三条
    const auto savedRecipes = repository.findByUserAndType(
        QStringLiteral("U001"), RecommendationItemType::Recipe);
    if (!savedRecipes.ok || savedRecipes.data.size() != 3) return 24;

    return 0;
}
