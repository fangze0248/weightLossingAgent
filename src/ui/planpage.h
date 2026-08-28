#pragma once

#include "models/PlanModels.h"

#include <QWidget>

class IPlanRepository;
class IPlanGenerationService;
class QDateEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class SessionManager;

class PlanPage : public QWidget
{
    Q_OBJECT

public:
    explicit PlanPage(IPlanRepository& repository,
                      IPlanGenerationService& generationService,
                      SessionManager& sessionManager,
                      QWidget* parent = nullptr);

    void displayPlan(const WeeklyPlan& plan);

signals:
    void planChanged();

private:
    void generateWeeklyPlan();
    void loadLatestPlan();
    void clearPlan();
    void updateCurrentUser(const QString& userId);

    IPlanRepository& repository_;
    IPlanGenerationService& generationService_;
    SessionManager& sessionManager_;
    QDateEdit* startDateEdit_ = nullptr;
    QPushButton* generateButton_ = nullptr;
    QLabel* currentUserLabel_ = nullptr;
    QPushButton* loadButton_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QTableWidget* planTable_ = nullptr;
};
