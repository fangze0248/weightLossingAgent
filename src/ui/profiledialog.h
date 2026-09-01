#pragma once

#include "models/UserProfile.h"

#include <QDialog>

class IUserRepository;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

class ProfileDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        Create,
        Edit
    };

    explicit ProfileDialog(IUserRepository& repository,
                           Mode mode,
                           QWidget* parent = nullptr);

    void setUser(const UserProfile& user);
    UserProfile savedUser() const;

private:
    UserProfile buildUser() const;
    void saveProfile();

    IUserRepository& repository_;
    Mode mode_;
    UserProfile savedUser_;
    QLineEdit* idEdit_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* genderCombo_ = nullptr;
    QComboBox* exerciseGoalCombo_ = nullptr;
    QSpinBox* ageSpin_ = nullptr;
    QDoubleSpinBox* heightSpin_ = nullptr;
    QDoubleSpinBox* weightSpin_ = nullptr;
    QDoubleSpinBox* targetWeightSpin_ = nullptr;
    QSpinBox* averageDailyStepsSpin_ = nullptr;
    QDoubleSpinBox* weeklyGoalSpin_ = nullptr;
    QSpinBox* dietRatioSpin_ = nullptr;
    QPushButton* saveButton_ = nullptr;
};
