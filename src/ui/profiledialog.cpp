#include "ui/profiledialog.h"

#include "interfaces/IUserRepository.h"

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
#include <QtGlobal>

ProfileDialog::ProfileDialog(IUserRepository& repository,
                             Mode mode,
                             QWidget* parent)
    : QDialog(parent), repository_(repository), mode_(mode)
{
    setWindowTitle(mode_ == Mode::Create
                       ? QStringLiteral("新建账号")
                       : QStringLiteral("修改个人信息"));
    setModal(true);
    resize(520, 650);

    auto* titleLabel = new QLabel(windowTitle(), this);
    titleLabel->setProperty("role", "pageTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    auto* hintLabel = new QLabel(
        mode_ == Mode::Create
            ? QStringLiteral("创建账号后将自动登录，用户编号不可与已有账号重复。")
            : QStringLiteral("用户编号不可修改，其他健康数据保存后立即生效。"),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setProperty("role", "subtitle");

    idEdit_ = new QLineEdit(this);
    idEdit_->setPlaceholderText(QStringLiteral("例如：U002"));
    idEdit_->setReadOnly(mode_ == Mode::Edit);
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QStringLiteral("输入用户名称"));

    genderCombo_ = new QComboBox(this);
    genderCombo_->addItem(QStringLiteral("男"), static_cast<int>(Gender::Male));
    genderCombo_->addItem(QStringLiteral("女"), static_cast<int>(Gender::Female));

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

    averageDailyStepsSpin_ = new QSpinBox(this);
    averageDailyStepsSpin_->setRange(0, 50000);
    averageDailyStepsSpin_->setSingleStep(500);
    averageDailyStepsSpin_->setValue(4000);
    averageDailyStepsSpin_->setSuffix(QStringLiteral(" 步/天"));
    averageDailyStepsSpin_->setToolTip(QStringLiteral(
        "请输入过去 7 天的日均步数；健身等专项锻炼由运动处方另外安排。"));

    weeklyGoalSpin_ = new QDoubleSpinBox(this);
    weeklyGoalSpin_->setRange(0.1, 1.5);
    weeklyGoalSpin_->setSingleStep(0.1);
    weeklyGoalSpin_->setValue(0.5);
    weeklyGoalSpin_->setSuffix(QStringLiteral(" kg/周"));

    dietRatioSpin_ = new QSpinBox(this);
    dietRatioSpin_->setRange(0, 100);
    dietRatioSpin_->setValue(70);
    dietRatioSpin_->setSuffix(QStringLiteral(" %"));

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
    formLayout->addRow(QStringLiteral("当前体重："), weightSpin_);
    formLayout->addRow(QStringLiteral("目标体重："), targetWeightSpin_);
    formLayout->addRow(QStringLiteral("过去7天日均步数："),
                       averageDailyStepsSpin_);
    formLayout->addRow(QStringLiteral("每周减重目标："), weeklyGoalSpin_);
    formLayout->addRow(QStringLiteral("饮食贡献比例："), dietRatioSpin_);

    saveButton_ = new QPushButton(
        mode_ == Mode::Create
            ? QStringLiteral("创建账号并登录")
            : QStringLiteral("保存修改"),
        this);
    saveButton_->setProperty("variant", "primary");
    auto* cancelButton = new QPushButton(QStringLiteral("取消"), this);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(saveButton_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 20);
    layout->setSpacing(12);
    layout->addWidget(titleLabel);
    layout->addWidget(hintLabel);
    layout->addWidget(formCard);
    layout->addLayout(buttonLayout);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveButton_, &QPushButton::clicked, this, &ProfileDialog::saveProfile);
}

void ProfileDialog::setUser(const UserProfile& user)
{
    idEdit_->setText(user.id);
    nameEdit_->setText(user.name);
    const int genderIndex = genderCombo_->findData(static_cast<int>(user.gender));
    if (genderIndex >= 0) genderCombo_->setCurrentIndex(genderIndex);
    ageSpin_->setValue(user.age);
    heightSpin_->setValue(user.heightCm);
    weightSpin_->setValue(user.weightKg);
    targetWeightSpin_->setValue(user.targetWeightKg);
    averageDailyStepsSpin_->setValue(user.averageDailySteps);
    weeklyGoalSpin_->setValue(user.weeklyGoalKg);
    dietRatioSpin_->setValue(qRound(user.dietContributionRatio * 100.0));
}

UserProfile ProfileDialog::savedUser() const
{
    return savedUser_;
}

UserProfile ProfileDialog::buildUser() const
{
    UserProfile user;
    user.id = idEdit_->text().trimmed();
    user.name = nameEdit_->text().trimmed();
    user.gender = static_cast<Gender>(genderCombo_->currentData().toInt());
    user.age = ageSpin_->value();
    user.heightCm = heightSpin_->value();
    user.weightKg = weightSpin_->value();
    user.targetWeightKg = targetWeightSpin_->value();
    user.averageDailySteps = averageDailyStepsSpin_->value();
    user.goalType = GoalType::Lose;
    user.weeklyGoalKg = weeklyGoalSpin_->value();
    user.dietContributionRatio = dietRatioSpin_->value() / 100.0;
    return user;
}

void ProfileDialog::saveProfile()
{
    const UserProfile user = buildUser();
    if (user.id.isEmpty() || user.name.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("保存失败"),
                             QStringLiteral("用户编号和名称不能为空"));
        return;
    }

    if (mode_ == Mode::Create) {
        const auto existing = repository_.findById(user.id);
        if (!existing.ok) {
            QMessageBox::warning(this,
                                 QStringLiteral("查询失败"),
                                 existing.message);
            return;
        }
        if (existing.data.has_value()) {
            QMessageBox::warning(this,
                                 QStringLiteral("创建失败"),
                                 QStringLiteral("该用户编号已存在"));
            return;
        }
    }

    const auto result = mode_ == Mode::Create
                            ? repository_.add(user)
                            : repository_.update(user);
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("保存失败"),
                             result.message);
        return;
    }

    savedUser_ = result.data;
    accept();
}
