#include "ui/feedbackdialog.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUuid>
#include <QVBoxLayout>

namespace {

QComboBox* makeStarsCombo(QWidget* parent)
{
    auto* combo = new QComboBox(parent);
    combo->addItem(QStringLiteral("未体验"), 0);
    combo->addItem(QStringLiteral("★"), 1);
    combo->addItem(QStringLiteral("★★"), 2);
    combo->addItem(QStringLiteral("★★★"), 3);
    combo->addItem(QStringLiteral("★★★★"), 4);
    combo->addItem(QStringLiteral("★★★★★"), 5);
    combo->setMinimumWidth(120);
    return combo;
}

} // namespace

FeedbackDialog::FeedbackDialog(const QString& userId,
                               const QString& planId,
                               const DailyPlan& day,
                               QWidget* parent)
    : QDialog(parent),
      userId_(userId),
      planId_(planId),
      date_(day.date)
{
    setWindowTitle(QStringLiteral("打卡反馈"));
    setModal(true);
    setMinimumWidth(480);

    auto* titleLabel = new QLabel(QStringLiteral("今天体验如何？"), this);
    titleLabel->setProperty("role", "pageTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    auto* hintLabel = new QLabel(
        QStringLiteral("对今天体验过的运动和食谱打分，未体验的项保持「未体验」即可。"),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setProperty("role", "subtitle");
    hintLabel->setAlignment(Qt::AlignCenter);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    auto* scrollContent = new QWidget(scrollArea);
    rowsLayout_ = new QVBoxLayout(scrollContent);
    rowsLayout_->setContentsMargins(4, 4, 4, 4);
    rowsLayout_->setSpacing(8);

    // 运动享受度
    if (!day.exercises.isEmpty()) {
        auto* exerciseHeader =
            new QLabel(QStringLiteral("运动享受度"), scrollContent);
        exerciseHeader->setProperty("role", "sectionTitle");
        rowsLayout_->addWidget(exerciseHeader);
        for (const ExercisePlanItem& item : day.exercises) {
            addRatingRow(
                QStringLiteral("运动 · %1（%2 分钟）")
                    .arg(item.exerciseName)
                    .arg(item.durationMinutes),
                RecommendationItemType::Exercise,
                item.exerciseId,
                {});
        }
    }

    // 饮食享受度
    const auto addMealGroup = [&](const QString& title,
                                  const QVector<MealPlanItem>& items) {
        for (const MealPlanItem& item : items) {
            addRatingRow(QStringLiteral("%1 · %2")
                             .arg(title, item.recipeName),
                         RecommendationItemType::Recipe,
                         item.recipeId,
                         item.nutritionTags);
        }
    };
    const bool hasMeals = !day.meals.breakfast.isEmpty()
        || !day.meals.lunch.isEmpty()
        || !day.meals.dinner.isEmpty()
        || !day.meals.snacks.isEmpty();
    if (hasMeals) {
        auto* mealHeader =
            new QLabel(QStringLiteral("饮食享受度"), scrollContent);
        mealHeader->setProperty("role", "sectionTitle");
        rowsLayout_->addWidget(mealHeader);
        addMealGroup(QStringLiteral("早餐"), day.meals.breakfast);
        addMealGroup(QStringLiteral("午餐"), day.meals.lunch);
        addMealGroup(QStringLiteral("晚餐"), day.meals.dinner);
        addMealGroup(QStringLiteral("加餐"), day.meals.snacks);
    }

    if (rows_.isEmpty()) {
        rowsLayout_->addWidget(new QLabel(
            QStringLiteral("当天没有可反馈的运动或食谱。"), scrollContent));
    }

    rowsLayout_->addStretch();
    scrollArea->setWidget(scrollContent);

    auto* submitButton = new QPushButton(QStringLiteral("提交反馈"), this);
    submitButton->setProperty("variant", "primary");
    auto* skipButton = new QPushButton(QStringLiteral("跳过"), this);
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(skipButton);
    buttonLayout->addWidget(submitButton);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 20);
    layout->setSpacing(12);
    layout->addWidget(titleLabel);
    layout->addWidget(hintLabel);
    layout->addWidget(scrollArea, 1);
    layout->addLayout(buttonLayout);

    connect(skipButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(submitButton, &QPushButton::clicked, this, &QDialog::accept);
}

void FeedbackDialog::addRatingRow(const QString& caption,
                                  RecommendationItemType itemType,
                                  const QString& itemId,
                                  const QStringList& keywords)
{
    auto* label = new QLabel(caption, this);
    label->setWordWrap(true);
    auto* combo = makeStarsCombo(this);

    auto* rowLayout = new QHBoxLayout;
    rowLayout->addWidget(label, 1);
    rowLayout->addWidget(combo);
    rowsLayout_->addLayout(rowLayout);

    rows_.append({itemType, itemId, keywords, combo});
}

QVector<Feedback> FeedbackDialog::buildFeedback() const
{
    QVector<Feedback> result;
    for (const RatingRow& row : rows_) {
        const int stars = row.combo->currentData().toInt();
        if (stars < 1 || stars > 5) {
            continue; // 未体验不产生反馈
        }
        Feedback feedback;
        feedback.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        feedback.userId = userId_;
        feedback.itemType = row.itemType;
        feedback.itemId = row.itemId;
        feedback.enjoymentStars = stars;
        feedback.keywords = row.keywords;
        feedback.planId = planId_;
        feedback.feedbackDate = date_;
        result.append(feedback);
    }
    return result;
}

QVector<Feedback> FeedbackDialog::collectedFeedback() const
{
    return buildFeedback();
}
