#include "ui/dashboardpage.h"

#include "interfaces/IHealthCalculator.h"
#include "interfaces/IFeedbackService.h"
#include "interfaces/IPlanRepository.h"
#include "interfaces/IUserRepository.h"
#include "session/sessionmanager.h"
#include "ui/feedbackdialog.h"
#include "ui/profiledialog.h"

#include <QAbstractItemView>
#include <QDate>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {

QString exerciseText(const QVector<ExercisePlanItem>& exercises)
{
    if (exercises.isEmpty()) {
        return QStringLiteral("今日没有安排运动，可进行轻松步行或充分休息。");
    }

    QStringList lines;
    for (const ExercisePlanItem& item : exercises) {
        lines.append(QStringLiteral("• %1　%2 分钟　预计消耗 %3 kcal")
                         .arg(item.exerciseName)
                         .arg(item.durationMinutes)
                         .arg(item.caloriesBurned, 0, 'f', 0));
    }
    return lines.join(QLatin1Char('\n'));
}

QString mealGroupText(const QString& title,
                      const QVector<MealPlanItem>& meals)
{
    if (meals.isEmpty()) {
        return QStringLiteral("%1：暂无安排").arg(title);
    }

    QStringList names;
    for (const MealPlanItem& item : meals) {
        names.append(QStringLiteral("%1（%2 kcal）")
                         .arg(item.recipeName)
                         .arg(item.calories, 0, 'f', 0));
    }
    return QStringLiteral("%1：%2").arg(title, names.join(QStringLiteral("、")));
}

QString mealText(const MealPlan& meals)
{
    return QStringList{
        mealGroupText(QStringLiteral("早餐"), meals.breakfast),
        mealGroupText(QStringLiteral("午餐"), meals.lunch),
        mealGroupText(QStringLiteral("晚餐"), meals.dinner),
        mealGroupText(QStringLiteral("加餐"), meals.snacks)
    }.join(QLatin1Char('\n'));
}

QTableWidgetItem* readonlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

} // namespace

DashboardPage::DashboardPage(IUserRepository& userRepository,
                             IPlanRepository& planRepository,
                             IHealthCalculator& healthCalculator,
                             IFeedbackService& feedbackService,
                             SessionManager& sessionManager,
                             QWidget* parent)
    : QWidget(parent),
      userRepository_(userRepository),
      planRepository_(planRepository),
      healthCalculator_(healthCalculator),
      feedbackService_(feedbackService),
      sessionManager_(sessionManager)
{
    setProperty("page", true);

    auto* profileCard = new QFrame(this);
    profileCard->setObjectName(QStringLiteral("profileSidebar"));
    profileCard->setProperty("card", true);
    profileCard->setMinimumWidth(255);
    profileCard->setMaximumWidth(300);

    avatarLabel_ = new QLabel(QStringLiteral("用"), profileCard);
    avatarLabel_->setObjectName(QStringLiteral("avatarCircle"));
    avatarLabel_->setAlignment(Qt::AlignCenter);
    avatarLabel_->setFixedSize(82, 82);
    nameLabel_ = new QLabel(QStringLiteral("未登录"), profileCard);
    nameLabel_->setObjectName(QStringLiteral("profileName"));
    nameLabel_->setAlignment(Qt::AlignCenter);
    basicInfoLabel_ = new QLabel(profileCard);
    basicInfoLabel_->setAlignment(Qt::AlignCenter);
    basicInfoLabel_->setProperty("role", "subtitle");

    auto* bmiCard = new QFrame(profileCard);
    bmiCard->setObjectName(QStringLiteral("metricCard"));
    auto* bmiCaption = new QLabel(QStringLiteral("BMI"), bmiCard);
    bmiCaption->setProperty("role", "metricCaption");
    bmiValueLabel_ = new QLabel(QStringLiteral("--"), bmiCard);
    bmiValueLabel_->setProperty("role", "metricValue");
    auto* bmiLayout = new QVBoxLayout(bmiCard);
    bmiLayout->setContentsMargins(16, 12, 16, 12);
    bmiLayout->addWidget(bmiCaption);
    bmiLayout->addWidget(bmiValueLabel_);

    metabolismLabel_ = new QLabel(profileCard);
    metabolismLabel_->setWordWrap(true);
    metabolismLabel_->setProperty("role", "infoBlock");
    goalLabel_ = new QLabel(profileCard);
    goalLabel_->setWordWrap(true);
    goalLabel_->setProperty("role", "goalBlock");

    auto* editButton = new QPushButton(QStringLiteral("修改个人信息"), profileCard);
    editButton->setProperty("compact", true);
    auto* logoutButton = new QPushButton(QStringLiteral("退出登录"), profileCard);
    logoutButton->setProperty("compact", true);
    logoutButton->setProperty("variant", "danger");
    auto* profileButtonLayout = new QHBoxLayout;
    profileButtonLayout->setSpacing(8);
    profileButtonLayout->addWidget(editButton);
    profileButtonLayout->addWidget(logoutButton);

    auto* profileLayout = new QVBoxLayout(profileCard);
    profileLayout->setContentsMargins(20, 20, 20, 20);
    profileLayout->setSpacing(12);
    profileLayout->addWidget(avatarLabel_, 0, Qt::AlignHCenter);
    profileLayout->addWidget(nameLabel_);
    profileLayout->addWidget(basicInfoLabel_);
    profileLayout->addSpacing(4);
    profileLayout->addWidget(bmiCard);
    profileLayout->addWidget(metabolismLabel_);
    profileLayout->addWidget(goalLabel_);
    profileLayout->addStretch();
    profileLayout->addLayout(profileButtonLayout);

    auto* recommendationCard = new QFrame(this);
    recommendationCard->setProperty("card", true);
    auto* recommendationTitle = new QLabel(
        QStringLiteral("今日智能推荐"), recommendationCard);
    recommendationTitle->setProperty("role", "pageTitle");
    dayTitleLabel_ = new QLabel(
        QStringLiteral("等待加载计划"), recommendationCard);
    dayTitleLabel_->setProperty("role", "sectionTitle");
    exerciseRecommendationLabel_ = new QLabel(recommendationCard);
    exerciseRecommendationLabel_->setWordWrap(true);
    exerciseRecommendationLabel_->setProperty("recommendation", "exercise");
    mealRecommendationLabel_ = new QLabel(recommendationCard);
    mealRecommendationLabel_->setWordWrap(true);
    mealRecommendationLabel_->setProperty("recommendation", "meal");
    recommendationSummaryLabel_ = new QLabel(recommendationCard);
    recommendationSummaryLabel_->setWordWrap(true);
    recommendationSummaryLabel_->setProperty("role", "summaryBlock");

    auto* recommendationLayout = new QVBoxLayout(recommendationCard);
    recommendationLayout->setContentsMargins(20, 18, 20, 18);
    recommendationLayout->setSpacing(12);
    recommendationLayout->addWidget(recommendationTitle);
    recommendationLayout->addWidget(dayTitleLabel_);
    recommendationLayout->addWidget(exerciseRecommendationLabel_);
    recommendationLayout->addWidget(mealRecommendationLabel_);
    recommendationLayout->addWidget(recommendationSummaryLabel_);
    recommendationLayout->addStretch();

    auto* planCard = new QFrame(this);
    planCard->setProperty("card", true);
    planCard->setMinimumWidth(350);
    auto* planTitle = new QLabel(QStringLiteral("本周计划打卡"), planCard);
    planTitle->setProperty("role", "pageTitle");
    planPeriodLabel_ = new QLabel(QStringLiteral("尚无计划"), planCard);
    planPeriodLabel_->setProperty("role", "subtitle");
    checkInTable_ = new QTableWidget(0, 4, planCard);
    checkInTable_->setHorizontalHeaderLabels({
        QStringLiteral("日期"),
        QStringLiteral("运动"),
        QStringLiteral("饮食"),
        QStringLiteral("状态")
    });
    checkInTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    checkInTable_->verticalHeader()->setVisible(false);
    checkInTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    checkInTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    checkInTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    checkInTable_->setAlternatingRowColors(true);
    checkInButton_ = new QPushButton(QStringLiteral("完成所选日期打卡"), planCard);
    checkInButton_->setProperty("variant", "primary");
    checkInButton_->setEnabled(false);

    auto* planLayout = new QVBoxLayout(planCard);
    planLayout->setContentsMargins(18, 18, 18, 18);
    planLayout->setSpacing(10);
    planLayout->addWidget(planTitle);
    planLayout->addWidget(planPeriodLabel_);
    planLayout->addWidget(checkInTable_, 1);
    planLayout->addWidget(checkInButton_);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(12);
    layout->addWidget(profileCard);
    layout->addWidget(recommendationCard, 1);
    layout->addWidget(planCard);

    connect(editButton,
            &QPushButton::clicked,
            this,
            &DashboardPage::editProfile);
    connect(logoutButton,
            &QPushButton::clicked,
            this,
            &DashboardPage::logoutRequested);
    connect(checkInButton_,
            &QPushButton::clicked,
            this,
            &DashboardPage::checkInSelectedDay);
    connect(checkInTable_,
            &QTableWidget::currentCellChanged,
            this,
            [this](int currentRow, int, int, int) {
                displaySelectedDay(currentRow);
            });
}

void DashboardPage::refresh()
{
    currentUser_.reset();
    currentPlan_.reset();

    const QString userId = sessionManager_.currentUserId();
    if (userId.isEmpty()) {
        clearDashboard(QStringLiteral("当前未登录"));
        return;
    }

    const auto userResult = userRepository_.findById(userId);
    if (!userResult.ok || !userResult.data.has_value()) {
        clearDashboard(userResult.ok
                           ? QStringLiteral("当前账号不存在")
                           : userResult.message);
        return;
    }
    currentUser_ = *userResult.data;

    const auto calorieResult = healthCalculator_.calculate(*currentUser_);
    if (calorieResult.ok) {
        updateProfilePanel(*currentUser_, calorieResult.data);
    } else {
        CalorieNeed emptyNeed;
        updateProfilePanel(*currentUser_, emptyNeed);
        metabolismLabel_->setText(
            QStringLiteral("健康指标计算失败：%1").arg(calorieResult.message));
    }

    const auto planResult = planRepository_.findByUserId(userId);
    if (!planResult.ok) {
        planPeriodLabel_->setText(
            QStringLiteral("计划加载失败：%1").arg(planResult.message));
    } else if (!planResult.data.isEmpty()) {
        currentPlan_ = planResult.data.first();
    }
    updatePlanPanel();
}

void DashboardPage::editProfile()
{
    if (!currentUser_.has_value()) {
        return;
    }

    ProfileDialog dialog(userRepository_, ProfileDialog::Mode::Edit, this);
    dialog.setUser(*currentUser_);
    if (dialog.exec() == QDialog::Accepted) {
        refresh();
    }
}

void DashboardPage::checkInSelectedDay()
{
    if (!currentPlan_.has_value()) {
        return;
    }

    const int row = checkInTable_->currentRow();
    if (row < 0 || row >= currentPlan_->days.size()) {
        QMessageBox::information(this,
                                 QStringLiteral("请选择日期"),
                                 QStringLiteral("请先选择需要打卡的一天"));
        return;
    }

    currentPlan_->days[row].completed = true;
    const auto saveResult = planRepository_.save(*currentPlan_);
    if (!saveResult.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("打卡失败"),
                             saveResult.message);
        return;
    }

    currentPlan_ = saveResult.data;
    const DailyPlan& day = currentPlan_->days.at(row);
    updatePlanPanel();
    checkInTable_->selectRow(row);

    // 打卡成功后弹出享受度反馈板块，未体验的项不会写入数据库。
    FeedbackDialog dialog(sessionManager_.currentUserId(),
                          currentPlan_->planId,
                          day,
                          this);
    if (dialog.exec() == QDialog::Accepted) {
        const QVector<Feedback> items = dialog.collectedFeedback();
        int savedCount = 0;
        for (const Feedback& feedback : items) {
            if (feedbackService_.record(feedback).ok) {
                ++savedCount;
            }
        }
        if (savedCount < items.size()) {
            QMessageBox::warning(this,
                                 QStringLiteral("反馈保存部分失败"),
                                 QStringLiteral("部分反馈未能保存，可稍后重试。"));
        }
    }

    QMessageBox::information(this,
                             QStringLiteral("打卡成功"),
                             QStringLiteral("当天计划已标记为完成"));
}

void DashboardPage::displaySelectedDay(int row)
{
    if (!currentPlan_.has_value()
        || row < 0
        || row >= currentPlan_->days.size()) {
        checkInButton_->setEnabled(false);
        return;
    }

    const DailyPlan& day = currentPlan_->days.at(row);
    dayTitleLabel_->setText(
        QStringLiteral("%1 的运动与食谱")
            .arg(day.date.toString(QStringLiteral("yyyy-MM-dd"))));
    exerciseRecommendationLabel_->setText(
        QStringLiteral("🏃 运动推荐\n%1").arg(exerciseText(day.exercises)));
    mealRecommendationLabel_->setText(
        QStringLiteral("🥗 食谱推荐\n%1").arg(mealText(day.meals)));
    recommendationSummaryLabel_->setText(
        QStringLiteral("建议摄入 %1 kcal　·　饮食合计 %2 kcal　·　运动消耗 %3 kcal")
            .arg(day.calorieNeed.recommendedIntake, 0, 'f', 0)
            .arg(day.meals.totalCalories, 0, 'f', 0)
            .arg(day.totalCaloriesBurned, 0, 'f', 0));
    checkInButton_->setEnabled(!day.completed);
    checkInButton_->setText(day.completed
                                ? QStringLiteral("该日期已完成打卡")
                                : QStringLiteral("完成所选日期打卡"));
}

void DashboardPage::clearDashboard(const QString& message)
{
    avatarLabel_->setText(QStringLiteral("用"));
    nameLabel_->setText(QStringLiteral("未登录"));
    basicInfoLabel_->setText(message);
    bmiValueLabel_->setText(QStringLiteral("--"));
    metabolismLabel_->clear();
    goalLabel_->clear();
    planPeriodLabel_->setText(QStringLiteral("尚无计划"));
    checkInTable_->setRowCount(0);
    checkInButton_->setEnabled(false);
    dayTitleLabel_->setText(QStringLiteral("等待加载计划"));
    exerciseRecommendationLabel_->setText(
        QStringLiteral("登录后显示运动推荐"));
    mealRecommendationLabel_->setText(QStringLiteral("登录后显示食谱推荐"));
    recommendationSummaryLabel_->clear();
}

void DashboardPage::updateProfilePanel(const UserProfile& user,
                                       const CalorieNeed& calorieNeed)
{
    const QString displayName = user.name.isEmpty() ? user.id : user.name;
    avatarLabel_->setText(displayName.left(1));
    nameLabel_->setText(displayName);
    basicInfoLabel_->setText(
        QStringLiteral("%1 · %2 岁 · %3 cm\n当前体重 %4 kg · 日均 %5 步")
            .arg(user.gender == Gender::Male
                     ? QStringLiteral("男")
                     : QStringLiteral("女"))
            .arg(user.age)
            .arg(user.heightCm, 0, 'f', 1)
            .arg(user.weightKg, 0, 'f', 1)
            .arg(user.averageDailySteps));
    bmiValueLabel_->setText(
        QStringLiteral("%1  %2")
            .arg(calorieNeed.bmi, 0, 'f', 1)
            .arg(calorieNeed.bmiEvaluation));
    metabolismLabel_->setText(
        QStringLiteral("🔥 能量代谢\n基础代谢　%1 kcal\n基础生活总消耗　%2 kcal")
            .arg(calorieNeed.bmr, 0, 'f', 0)
            .arg(calorieNeed.tdee, 0, 'f', 0));
    goalLabel_->setText(
        QStringLiteral("🎯 减重目标\n目标体重　%1 kg\n建议摄入　%2 kcal\n运动目标　%3 kcal")
            .arg(user.targetWeightKg, 0, 'f', 1)
            .arg(calorieNeed.recommendedIntake, 0, 'f', 0)
            .arg(calorieNeed.exerciseTarget, 0, 'f', 0));
}

void DashboardPage::updatePlanPanel()
{
    checkInTable_->setRowCount(0);
    if (!currentPlan_.has_value() || currentPlan_->days.isEmpty()) {
        planPeriodLabel_->setText(
            QStringLiteral("暂无算法推荐计划"));
        dayTitleLabel_->setText(QStringLiteral("尚未生成推荐计划"));
        exerciseRecommendationLabel_->setText(
            QStringLiteral("等待推荐模块生成并保存 WeeklyPlan 后，这里会显示运动推荐。"));
        mealRecommendationLabel_->setText(
            QStringLiteral("等待推荐模块生成并保存 WeeklyPlan 后，这里会显示食谱推荐。"));
        recommendationSummaryLabel_->setText(
            QStringLiteral("当前展示的是空状态，不是虚构的算法结果。"));
        checkInButton_->setEnabled(false);
        return;
    }

    const QDate endDate = currentPlan_->days.last().date;
    planPeriodLabel_->setText(
        QStringLiteral("%1 至 %2")
            .arg(currentPlan_->startDate.toString(QStringLiteral("yyyy-MM-dd")),
                 endDate.toString(QStringLiteral("yyyy-MM-dd"))));
    checkInTable_->setRowCount(currentPlan_->days.size());

    int selectedRow = 0;
    const QDate today = QDate::currentDate();
    for (qsizetype row = 0; row < currentPlan_->days.size(); ++row) {
        const DailyPlan& day = currentPlan_->days.at(row);
        if (day.date == today) {
            selectedRow = static_cast<int>(row);
        }
        checkInTable_->setItem(
            row, 0, readonlyItem(day.date.toString(QStringLiteral("MM-dd"))));
        checkInTable_->setItem(
            row, 1, readonlyItem(QStringLiteral("%1 kcal")
                                     .arg(day.totalCaloriesBurned, 0, 'f', 0)));
        checkInTable_->setItem(
            row, 2, readonlyItem(QStringLiteral("%1 kcal")
                                     .arg(day.meals.totalCalories, 0, 'f', 0)));
        auto* statusItem = readonlyItem(
            day.completed ? QStringLiteral("✓ 已完成")
                          : QStringLiteral("未打卡"));
        statusItem->setForeground(
            day.completed ? QColor(QStringLiteral("#2e8b3c"))
                          : QColor(QStringLiteral("#7b857d")));
        checkInTable_->setItem(row, 3, statusItem);
    }

    checkInTable_->selectRow(selectedRow);
    displaySelectedDay(selectedRow);
}
