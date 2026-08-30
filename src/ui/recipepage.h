#pragma once

#include <QWidget>

class IRecipeRepository;
class IDataExchangeService;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QTableWidget;

class RecipePage : public QWidget
{
    Q_OBJECT

public:
    explicit RecipePage(IRecipeRepository& repository,
                        IDataExchangeService& dataExchangeService,
                        QWidget* parent = nullptr);

private:
    void refreshTable();
    void addRecipe();
    void deleteSelectedRecipe();
    void importDataset();

    IRecipeRepository& repository_;
    IDataExchangeService& dataExchangeService_;
    QLineEdit* idEdit_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* mealTypeCombo_ = nullptr;
    QDoubleSpinBox* caloriesSpin_ = nullptr;
    QLineEdit* ingredientsEdit_ = nullptr;
    QLineEdit* tagsEdit_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QPushButton* importButton_ = nullptr;
    QTableWidget* recipeTable_ = nullptr;
};
