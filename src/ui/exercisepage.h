#pragma once

#include <QWidget>

class IExerciseRepository;
class IDataExchangeService;
class QTableWidget;
class QLineEdit;
class QPushButton;
class ExercisePage : public QWidget
{
    Q_OBJECT

public:
    explicit ExercisePage(IDataExchangeService& dataExchangeService,
                          QWidget* parent = nullptr);
    void setRepository(IExerciseRepository* repository);

private:
    void refreshTable();
    void addExercise();
    void deleteSelectedExercise();
    void importDataset();

    IDataExchangeService& dataExchangeService_;
    IExerciseRepository* repository_ = nullptr;
    QTableWidget* exerciseTable_ = nullptr;
    QLineEdit* idEdit_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* metEdit_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QPushButton* importButton_ = nullptr;
};
