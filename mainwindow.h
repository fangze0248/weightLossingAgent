#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class IExerciseRepository;
class IDataExchangeService;
class IHealthCalculator;
class IFeedbackService;
class IPlanRepository;
class IPlanGenerationService;
class IRecipeRepository;
class IUserRepository;
class SessionManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(IExerciseRepository& exerciseRepository,
                        IRecipeRepository& recipeRepository,
                        IPlanRepository& planRepository,
                        IPlanGenerationService& planGenerationService,
                        IDataExchangeService& dataExchangeService,
                        IUserRepository& userRepository,
                        IHealthCalculator& healthCalculator,
                        IFeedbackService& feedbackService,
                        SessionManager& sessionManager,
                        QWidget *parent = nullptr);
    ~MainWindow() override;
};
#endif // MAINWINDOW_H
