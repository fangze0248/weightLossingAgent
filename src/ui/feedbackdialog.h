#pragma once

#include "models/Feedback.h"
#include "models/PlanModels.h"

#include <QDialog>
#include <QVector>

class QComboBox;
class QVBoxLayout;

/**
 * 打卡完成后的享受度反馈对话框：列出当天安排的运动项与食谱项，
 * 每项支持「未体验」或 1～5 星，提交后汇总成 Feedback 列表。
 */
class FeedbackDialog : public QDialog
{
    Q_OBJECT

public:
    FeedbackDialog(const QString& userId,
                   const QString& planId,
                   const DailyPlan& day,
                   QWidget* parent = nullptr);

    // 只返回用户打了星的项；未体验的项被忽略。
    QVector<Feedback> collectedFeedback() const;

private:
    struct RatingRow {
        RecommendationItemType itemType = RecommendationItemType::Exercise;
        QString itemId;
        QStringList keywords;
        QComboBox* combo = nullptr;
    };

    void addRatingRow(const QString& caption,
                      RecommendationItemType itemType,
                      const QString& itemId,
                      const QStringList& keywords);
    QVector<Feedback> buildFeedback() const;

    QString userId_;
    QString planId_;
    QDate date_;
    QVector<RatingRow> rows_;
    QVBoxLayout* rowsLayout_ = nullptr;
};
