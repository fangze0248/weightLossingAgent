#pragma once

#include <QWidget>

class IRecipeRepository;
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
                        QWidget* parent = nullptr);

private:
    void refreshTable();
    void addRecipe();
    void deleteSelectedRecipe();

    IRecipeRepository& repository_;
    QLineEdit* idEdit_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* mealTypeCombo_ = nullptr;
    QDoubleSpinBox* caloriesSpin_ = nullptr;
    QLineEdit* ingredientsEdit_ = nullptr;
    QLineEdit* tagsEdit_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QTableWidget* recipeTable_ = nullptr;
};
