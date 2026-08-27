#include "mainwindow.h"
#include "ui/exercisepage.h"
#include "ui/healthpage.h"
#include "ui/planpage.h"
#include "ui/recipepage.h"

#include <QTabWidget>

MainWindow::MainWindow(IExerciseRepository& exerciseRepository,
                       IRecipeRepository& recipeRepository,
                       IPlanRepository& planRepository,
                       IUserRepository& userRepository,
                       IHealthCalculator& healthCalculator,
                       SessionManager& sessionManager,
                       QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Weight Lossing Agent"));
    resize(800, 520);

    auto* tabs = new QTabWidget(this);
    auto* healthPage = new HealthPage(
        healthCalculator, userRepository, sessionManager, tabs);
    auto* exercisePage = new ExercisePage(tabs);
    exercisePage->setRepository(&exerciseRepository);
    auto* recipePage = new RecipePage(recipeRepository, tabs);
    auto* planPage = new PlanPage(planRepository, sessionManager, tabs);

    tabs->addTab(healthPage, QStringLiteral("健康计算"));
    tabs->addTab(exercisePage, QStringLiteral("运动管理"));
    tabs->addTab(recipePage, QStringLiteral("食谱管理"));
    tabs->addTab(planPage, QStringLiteral("计划结果"));
    setCentralWidget(tabs);
}

MainWindow::~MainWindow() = default;
