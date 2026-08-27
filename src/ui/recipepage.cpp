#include "ui/recipepage.h"

#include "interfaces/IRecipeRepository.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QString mealTypeText(MealType mealType)
{
    switch (mealType) {
    case MealType::Breakfast: return QStringLiteral("早餐");
    case MealType::Lunch: return QStringLiteral("午餐");
    case MealType::Dinner: return QStringLiteral("晚餐");
    case MealType::Snack: return QStringLiteral("加餐");
    }
    return QStringLiteral("未知");
}

QString ingredientsText(const QVector<Ingredient>& ingredients)
{
    QStringList parts;
    for (const Ingredient& ingredient : ingredients) {
        if (ingredient.amount > 0.0 || !ingredient.unit.isEmpty()) {
            parts.append(QStringLiteral("%1 %2 %3")
                             .arg(ingredient.name)
                             .arg(ingredient.amount, 0, 'f', 0)
                             .arg(ingredient.unit));
        } else {
            parts.append(ingredient.name);
        }
    }
    return parts.join(QStringLiteral("、"));
}

QStringList splitListText(const QString& text)
{
    QStringList values = text.split(
        QRegularExpression(QStringLiteral("[,，、]")),
        Qt::SkipEmptyParts);
    for (QString& value : values) {
        value = value.trimmed();
    }
    values.removeAll(QString{});
    return values;
}

} // namespace

RecipePage::RecipePage(IRecipeRepository& repository, QWidget* parent)
    : QWidget(parent), repository_(repository)
{
    auto* titleLabel = new QLabel(QStringLiteral("食谱数据管理"), this);
    titleLabel->setAlignment(Qt::AlignCenter);

    idEdit_ = new QLineEdit(this);
    idEdit_->setPlaceholderText(QStringLiteral("例如：R004"));
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QStringLiteral("例如：牛肉蔬菜沙拉"));

    mealTypeCombo_ = new QComboBox(this);
    mealTypeCombo_->addItem(QStringLiteral("早餐"),
                            static_cast<int>(MealType::Breakfast));
    mealTypeCombo_->addItem(QStringLiteral("午餐"),
                            static_cast<int>(MealType::Lunch));
    mealTypeCombo_->addItem(QStringLiteral("晚餐"),
                            static_cast<int>(MealType::Dinner));
    mealTypeCombo_->addItem(QStringLiteral("加餐"),
                            static_cast<int>(MealType::Snack));

    caloriesSpin_ = new QDoubleSpinBox(this);
    caloriesSpin_->setRange(0.0, 5000.0);
    caloriesSpin_->setValue(500.0);
    caloriesSpin_->setDecimals(0);
    caloriesSpin_->setSuffix(QStringLiteral(" kcal"));

    ingredientsEdit_ = new QLineEdit(this);
    ingredientsEdit_->setPlaceholderText(
        QStringLiteral("多个食材用逗号分隔，例如：牛肉，生菜"));
    tagsEdit_ = new QLineEdit(this);
    tagsEdit_->setPlaceholderText(
        QStringLiteral("多个标签用逗号分隔，例如：高蛋白，低脂"));

    addButton_ = new QPushButton(QStringLiteral("新增食谱"), this);
    deleteButton_ = new QPushButton(QStringLiteral("删除选中食谱"), this);
    refreshButton_ = new QPushButton(QStringLiteral("刷新食谱"), this);
    recipeTable_ = new QTableWidget(0, 6, this);
    recipeTable_->setHorizontalHeaderLabels({
        QStringLiteral("编号"),
        QStringLiteral("食谱名称"),
        QStringLiteral("餐别"),
        QStringLiteral("热量(kcal)"),
        QStringLiteral("食材"),
        QStringLiteral("营养标签")
    });
    recipeTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    recipeTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recipeTable_->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* formLayout = new QFormLayout;
    formLayout->addRow(QStringLiteral("食谱编号："), idEdit_);
    formLayout->addRow(QStringLiteral("食谱名称："), nameEdit_);
    formLayout->addRow(QStringLiteral("餐别："), mealTypeCombo_);
    formLayout->addRow(QStringLiteral("总热量："), caloriesSpin_);
    formLayout->addRow(QStringLiteral("食材："), ingredientsEdit_);
    formLayout->addRow(QStringLiteral("营养标签："), tagsEdit_);

    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(addButton_);
    buttonLayout->addWidget(deleteButton_);
    buttonLayout->addWidget(refreshButton_);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(titleLabel);
    layout->addLayout(formLayout);
    layout->addLayout(buttonLayout);
    layout->addWidget(recipeTable_);

    connect(addButton_,
            &QPushButton::clicked,
            this,
            &RecipePage::addRecipe);
    connect(refreshButton_,
            &QPushButton::clicked,
            this,
            &RecipePage::refreshTable);
    connect(deleteButton_,
            &QPushButton::clicked,
            this,
            &RecipePage::deleteSelectedRecipe);

    refreshTable();
}

void RecipePage::refreshTable()
{
    recipeTable_->setRowCount(0);

    const auto result = repository_.findAll();
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("读取失败"),
                             result.message);
        return;
    }

    recipeTable_->setRowCount(result.data.size());
    for (qsizetype row = 0; row < result.data.size(); ++row) {
        const Recipe& recipe = result.data.at(row);
        recipeTable_->setItem(row, 0, new QTableWidgetItem(recipe.id));
        recipeTable_->setItem(row, 1, new QTableWidgetItem(recipe.name));
        recipeTable_->setItem(
            row, 2, new QTableWidgetItem(mealTypeText(recipe.mealType)));
        recipeTable_->setItem(
            row, 3, new QTableWidgetItem(
                        QString::number(recipe.totalCalories, 'f', 0)));
        recipeTable_->setItem(
            row, 4, new QTableWidgetItem(ingredientsText(recipe.ingredients)));
        recipeTable_->setItem(
            row, 5, new QTableWidgetItem(
                        recipe.nutritionTags.join(QStringLiteral("、"))));
    }
}

void RecipePage::addRecipe()
{
    const QString id = idEdit_->text().trimmed();
    const QString name = nameEdit_->text().trimmed();
    if (id.isEmpty() || name.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("输入错误"),
            QStringLiteral("食谱编号和名称不能为空"));
        return;
    }

    Recipe recipe;
    recipe.id = id;
    recipe.name = name;
    recipe.mealType = static_cast<MealType>(
        mealTypeCombo_->currentData().toInt());
    recipe.totalCalories = caloriesSpin_->value();
    recipe.nutritionTags = splitListText(tagsEdit_->text());

    const QStringList ingredientNames = splitListText(ingredientsEdit_->text());
    for (const QString& ingredientName : ingredientNames) {
        Ingredient ingredient;
        ingredient.name = ingredientName;
        recipe.ingredients.append(ingredient);
    }

    const auto result = repository_.add(recipe);
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("新增失败"),
                             result.message);
        return;
    }

    idEdit_->clear();
    nameEdit_->clear();
    ingredientsEdit_->clear();
    tagsEdit_->clear();
    refreshTable();

    QMessageBox::information(
        this,
        QStringLiteral("新增成功"),
        QStringLiteral("食谱已保存到SQLite"));
}

void RecipePage::deleteSelectedRecipe()
{
    const int row = recipeTable_->currentRow();
    if (row < 0 || !recipeTable_->item(row, 0)) {
        QMessageBox::information(
            this,
            QStringLiteral("未选择数据"),
            QStringLiteral("请先在表格中选中一项食谱"));
        return;
    }

    const QString id = recipeTable_->item(row, 0)->text();
    const QString name = recipeTable_->item(row, 1)->text();
    if (QMessageBox::question(
            this,
            QStringLiteral("确认删除"),
            QStringLiteral("确定删除食谱“%1”（%2）吗？")
                .arg(name, id)) != QMessageBox::Yes) {
        return;
    }

    const auto result = repository_.remove(id);
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("删除失败"),
                             result.message);
        return;
    }
    if (!result.data) {
        QMessageBox::information(this,
                                 QStringLiteral("未删除"),
                                 QStringLiteral("该食谱已不存在"));
        refreshTable();
        return;
    }

    refreshTable();
    QMessageBox::information(this,
                             QStringLiteral("删除成功"),
                             QStringLiteral("食谱已从SQLite删除"));
}
