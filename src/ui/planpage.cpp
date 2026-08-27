#include "ui/planpage.h"

#include "interfaces/IPlanRepository.h"
#include "session/sessionmanager.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QString exerciseSummary(const QVector<ExercisePlanItem>& exercises)
{
    QStringList parts;
    for (const ExercisePlanItem& exercise : exercises) {
        parts.append(QStringLiteral("%1 %2分钟")
                         .arg(exercise.exerciseName)
                         .arg(exercise.durationMinutes));
    }
    return parts.join(QStringLiteral("、"));
}

QString mealSummary(const QVector<MealPlanItem>& meals)
{
    QStringList names;
    for (const MealPlanItem& meal : meals) {
        names.append(meal.recipeName);
    }
    return names.join(QStringLiteral("、"));
}

} // namespace

PlanPage::PlanPage(IPlanRepository& repository,
                   SessionManager& sessionManager,
                   QWidget* parent)
    : QWidget(parent),
      repository_(repository),
      sessionManager_(sessionManager)
{
    auto* titleLabel = new QLabel(QStringLiteral("周计划结果"), this);
    titleLabel->setAlignment(Qt::AlignCenter);

    currentUserLabel_ = new QLabel(this);
    currentUserLabel_->setText(
        sessionManager_.hasCurrentUser()
            ? QStringLiteral("当前账号：%1")
                  .arg(sessionManager_.currentUserId())
            : QStringLiteral("当前未登录"));
    loadButton_ = new QPushButton(QStringLiteral("加载最新计划"), this);

    auto* searchLayout = new QHBoxLayout;
    searchLayout->addWidget(currentUserLabel_);
    searchLayout->addStretch();
    searchLayout->addWidget(loadButton_);

    summaryLabel_ = new QLabel(QStringLiteral("尚未加载周计划"), this);
    summaryLabel_->setWordWrap(true);

    planTable_ = new QTableWidget(0, 8, this);
    planTable_->setHorizontalHeaderLabels({
        QStringLiteral("日期"),
        QStringLiteral("建议摄入"),
        QStringLiteral("运动安排"),
        QStringLiteral("运动消耗"),
        QStringLiteral("早餐"),
        QStringLiteral("午餐"),
        QStringLiteral("晚餐"),
        QStringLiteral("饮食总热量")
    });
    planTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    planTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    planTable_->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(titleLabel);
    layout->addLayout(searchLayout);
    layout->addWidget(summaryLabel_);
    layout->addWidget(planTable_);

    connect(loadButton_,
            &QPushButton::clicked,
            this,
            &PlanPage::loadLatestPlan);
    connect(&sessionManager_,
            &SessionManager::currentUserChanged,
            this,
            &PlanPage::updateCurrentUser);
}

void PlanPage::loadLatestPlan()
{
    const QString userId = sessionManager_.currentUserId();
    if (userId.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("未登录"),
                             QStringLiteral("请先在健康计算页登录或切换账号"));
        return;
    }

    const auto result = repository_.findByUserId(userId);
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("加载失败"),
                             result.message);
        return;
    }

    if (result.data.isEmpty()) {
        clearPlan();
        QMessageBox::information(
            this,
            QStringLiteral("暂无计划"),
            QStringLiteral("该用户还没有已保存的周计划"));
        return;
    }

    // Repository orders plans by start date descending, so the first is latest.
    displayPlan(result.data.first());
}

void PlanPage::displayPlan(const WeeklyPlan& plan)
{
    summaryLabel_->setText(
        QStringLiteral("计划编号：%1　用户：%2　开始日期：%3")
            .arg(plan.planId,
                 plan.userId,
                 plan.startDate.toString(QStringLiteral("yyyy-MM-dd"))));

    planTable_->setRowCount(plan.days.size());
    for (qsizetype row = 0; row < plan.days.size(); ++row) {
        const DailyPlan& day = plan.days.at(row);
        planTable_->setItem(
            row, 0, new QTableWidgetItem(
                        day.date.toString(QStringLiteral("yyyy-MM-dd"))));
        planTable_->setItem(
            row, 1, new QTableWidgetItem(
                        QString::number(
                            day.calorieNeed.recommendedIntake, 'f', 0)));
        planTable_->setItem(
            row, 2, new QTableWidgetItem(exerciseSummary(day.exercises)));
        planTable_->setItem(
            row, 3, new QTableWidgetItem(
                        QString::number(day.totalCaloriesBurned, 'f', 0)));
        planTable_->setItem(
            row, 4, new QTableWidgetItem(mealSummary(day.meals.breakfast)));
        planTable_->setItem(
            row, 5, new QTableWidgetItem(mealSummary(day.meals.lunch)));
        planTable_->setItem(
            row, 6, new QTableWidgetItem(mealSummary(day.meals.dinner)));
        planTable_->setItem(
            row, 7, new QTableWidgetItem(
                        QString::number(day.meals.totalCalories, 'f', 0)));
    }
}

void PlanPage::clearPlan()
{
    summaryLabel_->setText(QStringLiteral("尚未加载周计划"));
    planTable_->setRowCount(0);
}

void PlanPage::updateCurrentUser(const QString& userId)
{
    currentUserLabel_->setText(
        userId.isEmpty()
            ? QStringLiteral("当前未登录")
            : QStringLiteral("当前账号：%1").arg(userId));
    clearPlan();
}
