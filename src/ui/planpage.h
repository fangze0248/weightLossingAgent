#pragma once

#include "models/PlanModels.h"

#include <QWidget>

class IPlanRepository;
class QLabel;
class QPushButton;
class QTableWidget;
class SessionManager;

class PlanPage : public QWidget
{
    Q_OBJECT

public:
    explicit PlanPage(IPlanRepository& repository,
                      SessionManager& sessionManager,
                      QWidget* parent = nullptr);

    void displayPlan(const WeeklyPlan& plan);

private:
    void loadLatestPlan();
    void clearPlan();
    void updateCurrentUser(const QString& userId);

    IPlanRepository& repository_;
    SessionManager& sessionManager_;
    QLabel* currentUserLabel_ = nullptr;
    QPushButton* loadButton_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QTableWidget* planTable_ = nullptr;
};
