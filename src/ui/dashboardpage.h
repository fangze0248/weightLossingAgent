#pragma once

#include "models/PlanModels.h"
#include "models/UserProfile.h"

#include <QWidget>
#include <optional>

class IHealthCalculator;
class IFeedbackService;
class IPlanRepository;
class IUserRepository;
class QLabel;
class QPushButton;
class QTableWidget;
class SessionManager;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(IUserRepository& userRepository,
                           IPlanRepository& planRepository,
                           IHealthCalculator& healthCalculator,
                           IFeedbackService& feedbackService,
                           SessionManager& sessionManager,
                           QWidget* parent = nullptr);

    void refresh();

signals:
    void logoutRequested();

private:
    void editProfile();
    void checkInSelectedDay();
    void displaySelectedDay(int row);
    void clearDashboard(const QString& message);
    void updateProfilePanel(const UserProfile& user,
                            const CalorieNeed& calorieNeed);
    void updatePlanPanel();

    IUserRepository& userRepository_;
    IPlanRepository& planRepository_;
    IHealthCalculator& healthCalculator_;
    IFeedbackService& feedbackService_;
    SessionManager& sessionManager_;
    std::optional<UserProfile> currentUser_;
    std::optional<WeeklyPlan> currentPlan_;

    QLabel* avatarLabel_ = nullptr;
    QLabel* nameLabel_ = nullptr;
    QLabel* basicInfoLabel_ = nullptr;
    QLabel* bmiValueLabel_ = nullptr;
    QLabel* metabolismLabel_ = nullptr;
    QLabel* goalLabel_ = nullptr;
    QLabel* dayTitleLabel_ = nullptr;
    QLabel* exerciseRecommendationLabel_ = nullptr;
    QLabel* mealRecommendationLabel_ = nullptr;
    QLabel* recommendationSummaryLabel_ = nullptr;
    QLabel* planPeriodLabel_ = nullptr;
    QTableWidget* checkInTable_ = nullptr;
    QPushButton* checkInButton_ = nullptr;
};
