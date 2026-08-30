#pragma once

#include <QWidget>

class IHealthCalculator;
class IUserRepository;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class SessionManager;
struct UserProfile;

class HealthPage : public QWidget
{
    Q_OBJECT

public:
    explicit HealthPage(IHealthCalculator& calculator,
                        IUserRepository& userRepository,
                        SessionManager& sessionManager,
                        QWidget* parent = nullptr);

private:
    UserProfile buildUserProfile() const;
    void applyUserProfile(const UserProfile& user);
    bool loadUser(const QString& userId, bool showSuccessMessage);
    void loginOrSwitchUser();
    void updateCurrentUserLabel(const QString& userId);
    void calculateHealth();
    void saveUser();

    IHealthCalculator& calculator_;
    IUserRepository& userRepository_;
    SessionManager& sessionManager_;
    QLabel* currentUserLabel_ = nullptr;
    QLineEdit* idEdit_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* genderCombo_ = nullptr;
    QSpinBox* ageSpin_ = nullptr;
    QDoubleSpinBox* heightSpin_ = nullptr;
    QDoubleSpinBox* weightSpin_ = nullptr;
    QDoubleSpinBox* targetWeightSpin_ = nullptr;
    QComboBox* activityCombo_ = nullptr;
    QDoubleSpinBox* weeklyGoalSpin_ = nullptr;
    QSpinBox* dietRatioSpin_ = nullptr;
    QPushButton* calculateButton_ = nullptr;
    QPushButton* loginButton_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QLabel* resultLabel_ = nullptr;
};
