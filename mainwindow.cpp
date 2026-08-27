#include "mainwindow.h"
#include "ui/dashboardpage.h"
#include "ui/exercisepage.h"
#include "ui/loginpage.h"
#include "ui/planpage.h"
#include "ui/recipepage.h"
#include "interfaces/IUserRepository.h"
#include "session/sessionmanager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(IExerciseRepository& exerciseRepository,
                       IRecipeRepository& recipeRepository,
                       IPlanRepository& planRepository,
                       IUserRepository& userRepository,
                       IHealthCalculator& healthCalculator,
                       SessionManager& sessionManager,
                       QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("减重智能体"));
    resize(1220, 760);
    setMinimumSize(980, 640);

    auto* pageStack = new QStackedWidget(this);
    auto* loginPage = new LoginPage(userRepository, sessionManager, pageStack);
    auto* appRoot = new QWidget(pageStack);
    appRoot->setObjectName(QStringLiteral("appRoot"));
    auto* rootLayout = new QVBoxLayout(appRoot);
    rootLayout->setContentsMargins(18, 16, 18, 10);
    rootLayout->setSpacing(10);

    auto* header = new QFrame(appRoot);
    header->setObjectName(QStringLiteral("appHeader"));
    header->setMinimumHeight(76);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 12, 20, 12);

    auto* brandLayout = new QVBoxLayout;
    brandLayout->setSpacing(1);
    auto* brandTitle = new QLabel(QStringLiteral("⚖  减重智能体"), header);
    brandTitle->setObjectName(QStringLiteral("brandTitle"));
    auto* brandSubtitle = new QLabel(
        QStringLiteral("运动处方 · 食谱管理 · 周计划"), header);
    brandSubtitle->setObjectName(QStringLiteral("brandSubtitle"));
    brandLayout->addWidget(brandTitle);
    brandLayout->addWidget(brandSubtitle);

    auto* accountLabel = new QLabel(header);
    accountLabel->setObjectName(QStringLiteral("headerAccountBadge"));
    accountLabel->setText(
        sessionManager.hasCurrentUser()
            ? QStringLiteral("用户  %1  ▼").arg(sessionManager.currentUserId())
            : QStringLiteral("当前未登录"));

    headerLayout->addLayout(brandLayout);
    headerLayout->addStretch();
    headerLayout->addWidget(accountLabel);
    rootLayout->addWidget(header);

    auto* tabs = new QTabWidget(appRoot);
    tabs->setDocumentMode(true);
    tabs->tabBar()->setExpanding(false);
    auto* dashboardPage = new DashboardPage(userRepository,
                                            planRepository,
                                            healthCalculator,
                                            sessionManager,
                                            tabs);
    auto* exercisePage = new ExercisePage(tabs);
    exercisePage->setRepository(&exerciseRepository);
    auto* recipePage = new RecipePage(recipeRepository, tabs);
    auto* planPage = new PlanPage(planRepository, sessionManager, tabs);

    tabs->addTab(dashboardPage, QStringLiteral("主页"));
    tabs->addTab(exercisePage, QStringLiteral("运动管理"));
    tabs->addTab(recipePage, QStringLiteral("食谱管理"));
    tabs->addTab(planPage, QStringLiteral("计划结果"));
    rootLayout->addWidget(tabs, 1);

    pageStack->addWidget(loginPage);
    pageStack->addWidget(appRoot);
    setCentralWidget(pageStack);

    statusBar()->showMessage(
        QStringLiteral("就绪  |  SQLite 已连接  |  本地数据已同步"));

    connect(&sessionManager,
            &SessionManager::currentUserChanged,
            this,
            [accountLabel](const QString& userId) {
                accountLabel->setText(
                    userId.isEmpty()
                        ? QStringLiteral("当前未登录")
                        : QStringLiteral("用户  %1  ▼").arg(userId));
            });

    const auto showApplication = [this,
                                  pageStack,
                                  appRoot,
                                  dashboardPage]() {
        dashboardPage->refresh();
        pageStack->setCurrentWidget(appRoot);
        statusBar()->show();
    };
    const auto showLogin = [this,
                            pageStack,
                            loginPage]() {
        loginPage->prepareForDisplay();
        pageStack->setCurrentWidget(loginPage);
        statusBar()->hide();
    };

    connect(loginPage,
            &LoginPage::loginSucceeded,
            this,
            showApplication);
    connect(dashboardPage,
            &DashboardPage::logoutRequested,
            this,
            [&sessionManager, showLogin]() {
                sessionManager.clearCurrentUser();
                showLogin();
            });
    connect(tabs,
            &QTabWidget::currentChanged,
            this,
            [dashboardPage](int index) {
                if (index == 0) {
                    dashboardPage->refresh();
                }
            });

    bool validStoredSession = false;
    if (sessionManager.hasCurrentUser()) {
        const auto storedUser = userRepository.findById(
            sessionManager.currentUserId());
        validStoredSession = storedUser.ok && storedUser.data.has_value();
        if (!validStoredSession) {
            sessionManager.clearCurrentUser();
        }
    }

    if (validStoredSession) {
        showApplication();
    } else {
        showLogin();
    }
}

MainWindow::~MainWindow() = default;
