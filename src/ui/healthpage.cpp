#include "ui/healthpage.h"

#include "interfaces/IHealthCalculator.h"
#include "interfaces/IUserRepository.h"
#include "session/sessionmanager.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

HealthPage::HealthPage(IHealthCalculator& calculator,
                       IUserRepository& userRepository,
                       SessionManager& sessionManager,
                       QWidget* parent)
    : QWidget(parent),
      calculator_(calculator),
      userRepository_(userRepository),
      sessionManager_(sessionManager)
{
    setProperty("page", true);

    auto* titleLabel = new QLabel(
        QStringLiteral("用户健康数据与热量估算"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setProperty("role", "pageTitle");

    currentUserLabel_ = new QLabel(this);
    currentUserLabel_->setAlignment(Qt::AlignCenter);
    currentUserLabel_->setProperty("role", "currentUser");
    updateCurrentUserLabel(sessionManager_.currentUserId());

    idEdit_ = new QLineEdit(QStringLiteral("U001"), this);
    nameEdit_ = new QLineEdit(QStringLiteral("Demo User"), this);

    genderCombo_ = new QComboBox(this);
    genderCombo_->addItem(QStringLiteral("男"),
                          static_cast<int>(Gender::Male));
    genderCombo_->addItem(QStringLiteral("女"),
                          static_cast<int>(Gender::Female));

    ageSpin_ = new QSpinBox(this);
    ageSpin_->setRange(18, 100);
    ageSpin_->setValue(25);
    ageSpin_->setSuffix(QStringLiteral(" 岁"));

    heightSpin_ = new QDoubleSpinBox(this);
    heightSpin_->setRange(100.0, 230.0);
    heightSpin_->setValue(175.0);
    heightSpin_->setDecimals(1);
    heightSpin_->setSuffix(QStringLiteral(" cm"));

    weightSpin_ = new QDoubleSpinBox(this);
    weightSpin_->setRange(30.0, 300.0);
    weightSpin_->setValue(80.0);
    weightSpin_->setDecimals(1);
    weightSpin_->setSuffix(QStringLiteral(" kg"));

    targetWeightSpin_ = new QDoubleSpinBox(this);
    targetWeightSpin_->setRange(30.0, 300.0);
    targetWeightSpin_->setValue(70.0);
    targetWeightSpin_->setDecimals(1);
    targetWeightSpin_->setSuffix(QStringLiteral(" kg"));

    activityCombo_ = new QComboBox(this);
    activityCombo_->addItem(QStringLiteral("1 - 久坐"), 1);
    activityCombo_->addItem(QStringLiteral("2 - 轻度活动"), 2);
    activityCombo_->addItem(QStringLiteral("3 - 中度活动"), 3);
    activityCombo_->addItem(QStringLiteral("4 - 高度活动"), 4);
    activityCombo_->addItem(QStringLiteral("5 - 非常活跃"), 5);
    activityCombo_->setCurrentIndex(2);

    weeklyGoalSpin_ = new QDoubleSpinBox(this);
    weeklyGoalSpin_->setRange(0.1, 1.5);
    weeklyGoalSpin_->setSingleStep(0.1);
    weeklyGoalSpin_->setValue(0.5);
    weeklyGoalSpin_->setSuffix(QStringLiteral(" kg/周"));

    dietRatioSpin_ = new QSpinBox(this);
    dietRatioSpin_->setRange(0, 100);
    dietRatioSpin_->setValue(70);
    dietRatioSpin_->setSuffix(QStringLiteral(" %"));

    calculateButton_ = new QPushButton(
        QStringLiteral("计算健康指标"), this);
    calculateButton_->setProperty("variant", "primary");
    loginButton_ = new QPushButton(
        QStringLiteral("登录 / 切换到此账号"), this);
    saveButton_ = new QPushButton(
        QStringLiteral("保存用户资料"), this);
    saveButton_->setProperty("variant", "warning");

    resultLabel_ = new QLabel(
        QStringLiteral("请填写数据后点击计算"), this);
    resultLabel_->setWordWrap(true);
    resultLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultLabel_->setProperty("role", "resultCard");

    auto* formCard = new QFrame(this);
    formCard->setProperty("card", true);
    auto* formLayout = new QFormLayout(formCard);
    formLayout->setContentsMargins(22, 18, 22, 18);
    formLayout->setHorizontalSpacing(18);
    formLayout->setVerticalSpacing(10);
    formLayout->addRow(QStringLiteral("用户编号："), idEdit_);
    formLayout->addRow(QStringLiteral("用户名称："), nameEdit_);
    formLayout->addRow(QStringLiteral("性别："), genderCombo_);
    formLayout->addRow(QStringLiteral("年龄："), ageSpin_);
    formLayout->addRow(QStringLiteral("身高："), heightSpin_);
    formLayout->addRow(QStringLiteral("体重："), weightSpin_);
    formLayout->addRow(QStringLiteral("目标体重："), targetWeightSpin_);
    formLayout->addRow(QStringLiteral("活动等级："), activityCombo_);
    formLayout->addRow(QStringLiteral("每周减重目标："), weeklyGoalSpin_);
    formLayout->addRow(QStringLiteral("饮食贡献比例："), dietRatioSpin_);

    auto* actionLayout = new QHBoxLayout;
    actionLayout->setSpacing(10);
    actionLayout->addWidget(loginButton_);
    actionLayout->addWidget(calculateButton_);
    actionLayout->addWidget(saveButton_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 14, 22, 18);
    layout->setSpacing(11);
    layout->addWidget(titleLabel);
    layout->addWidget(currentUserLabel_);
    layout->addWidget(formCard);
    layout->addLayout(actionLayout);
    layout->addWidget(resultLabel_);
    layout->addStretch();

    connect(calculateButton_,
            &QPushButton::clicked,
            this,
            &HealthPage::calculateHealth);
    connect(loginButton_,
            &QPushButton::clicked,
            this,
            &HealthPage::loginOrSwitchUser);
    connect(saveButton_,
            &QPushButton::clicked,
            this,
            &HealthPage::saveUser);

    connect(&sessionManager_,
            &SessionManager::currentUserChanged,
            this,
            &HealthPage::updateCurrentUserLabel);

    if (sessionManager_.hasCurrentUser()) {
        loadUser(sessionManager_.currentUserId(), false);
    }
}

UserProfile HealthPage::buildUserProfile() const
{
    UserProfile user;
    user.id = idEdit_->text().trimmed();
    user.name = nameEdit_->text().trimmed();
    user.gender = static_cast<Gender>(genderCombo_->currentData().toInt());
    user.age = ageSpin_->value();
    user.heightCm = heightSpin_->value();
    user.weightKg = weightSpin_->value();
    user.targetWeightKg = targetWeightSpin_->value();
    user.activityLevel = activityCombo_->currentData().toInt();
    user.goalType = GoalType::Lose;
    user.weeklyGoalKg = weeklyGoalSpin_->value();
    user.dietContributionRatio = dietRatioSpin_->value() / 100.0;
    return user;
}

void HealthPage::applyUserProfile(const UserProfile& user)
{
    idEdit_->setText(user.id);
    nameEdit_->setText(user.name);

    const int genderIndex = genderCombo_->findData(
        static_cast<int>(user.gender));
    if (genderIndex >= 0) genderCombo_->setCurrentIndex(genderIndex);

    ageSpin_->setValue(user.age);
    heightSpin_->setValue(user.heightCm);
    weightSpin_->setValue(user.weightKg);
    targetWeightSpin_->setValue(user.targetWeightKg);

    const int activityIndex = activityCombo_->findData(user.activityLevel);
    if (activityIndex >= 0) activityCombo_->setCurrentIndex(activityIndex);

    weeklyGoalSpin_->setValue(user.weeklyGoalKg);
    dietRatioSpin_->setValue(
        qRound(user.dietContributionRatio * 100.0));
}

bool HealthPage::loadUser(const QString& userId, bool showSuccessMessage)
{
    const auto result = userRepository_.findById(userId.trimmed());
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("登录失败"),
                             result.message);
        return false;
    }

    if (!result.data.has_value()) {
        QMessageBox::warning(
            this,
            QStringLiteral("登录失败"),
            QStringLiteral("未找到用户编号 %1").arg(userId));
        if (sessionManager_.currentUserId() == userId.trimmed()) {
            sessionManager_.clearCurrentUser();
        }
        return false;
    }

    applyUserProfile(*result.data);
    sessionManager_.setCurrentUserId(result.data->id);

    if (showSuccessMessage) {
        QMessageBox::information(
            this,
            QStringLiteral("登录成功"),
            QStringLiteral("当前账号已切换为 %1（%2）")
                .arg(result.data->name, result.data->id));
    }
    return true;
}

void HealthPage::loginOrSwitchUser()
{
    const QString userId = idEdit_->text().trimmed();
    if (userId.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("登录失败"),
                             QStringLiteral("用户编号不能为空"));
        return;
    }
    loadUser(userId, true);
}

void HealthPage::updateCurrentUserLabel(const QString& userId)
{
    currentUserLabel_->setText(
        userId.isEmpty()
            ? QStringLiteral("当前未登录")
            : QStringLiteral("当前账号：%1").arg(userId));
}

void HealthPage::calculateHealth()
{
    const UserProfile user = buildUserProfile();

    const auto result = calculator_.calculate(user);
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("计算失败"),
                             result.message);
        return;
    }

    const CalorieNeed& need = result.data;
    QString text = QStringLiteral(
                       "BMI：%1（%2）\n"
                       "BMR：%3 kcal/天\n"
                       "TDEE：%4 kcal/天\n"
                       "每日热量缺口：%5 kcal\n"
                       "建议摄入：%6 kcal/天\n"
                       "运动消耗目标：%7 kcal/天")
                       .arg(need.bmi, 0, 'f', 1)
                       .arg(need.bmiEvaluation)
                       .arg(need.bmr, 0, 'f', 0)
                       .arg(need.tdee, 0, 'f', 0)
                       .arg(need.dailyDeficit, 0, 'f', 0)
                       .arg(need.recommendedIntake, 0, 'f', 0)
                       .arg(need.exerciseTarget, 0, 'f', 0);

    if (!result.warnings.isEmpty()) {
        text += QStringLiteral("\n\n提示：\n")
                + result.warnings.join(QLatin1Char('\n'));
    }
    resultLabel_->setText(text);
}

void HealthPage::saveUser()
{
    const UserProfile user = buildUserProfile();
    if (user.id.isEmpty() || user.name.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("用户编号和名称不能为空"));
        return;
    }

    const auto existingResult = userRepository_.findById(user.id);
    if (!existingResult.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("查询失败"),
                             existingResult.message);
        return;
    }

    const bool isUpdate = existingResult.data.has_value();
    const auto saveResult = isUpdate
                                ? userRepository_.update(user)
                                : userRepository_.add(user);
    if (!saveResult.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("保存失败"),
                             saveResult.message);
        return;
    }

    sessionManager_.setCurrentUserId(saveResult.data.id);

    QMessageBox::information(
        this,
        QStringLiteral("保存成功"),
        isUpdate
            ? QStringLiteral("用户资料已更新到SQLite")
            : QStringLiteral("用户资料已新增到SQLite"));
}
