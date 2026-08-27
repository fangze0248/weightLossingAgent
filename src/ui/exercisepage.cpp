#include "ui/exercisepage.h"
#include "interfaces/IExerciseRepository.h"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLineEdit>
ExercisePage::ExercisePage(QWidget* parent)
    : QWidget(parent)
{
    auto* messageLabel = new QLabel(
        QStringLiteral("这是我修改的第一个QT页面"), this);
    messageLabel->setAlignment(Qt::AlignCenter);
    auto* changeButton =new QPushButton(QStringLiteral("点击我"),this);
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(changeButton);
    layout->addWidget(messageLabel);
    connect(changeButton,
            &QPushButton::clicked,
            this,
            [messageLabel]()
            {messageLabel->setText(QStringLiteral("点击成功！"));});

    exerciseTable_ = new QTableWidget(0, 3, this);
    exerciseTable_->setHorizontalHeaderLabels({
        QStringLiteral("编号"),
        QStringLiteral("运动名称"),
         QStringLiteral("MET值")}
        );

    exerciseTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    idEdit_ = new QLineEdit(this);
    nameEdit_ = new QLineEdit(this);
    metEdit_ = new QLineEdit(this);
    addButton_ = new QPushButton(
        QStringLiteral("新增运动"), this);
    deleteButton_ = new QPushButton(
        QStringLiteral("删除选中运动"), this);

    idEdit_->setPlaceholderText(
        QStringLiteral("例如：EX004"));

    nameEdit_->setPlaceholderText(
        QStringLiteral("例如：游泳"));

    metEdit_->setPlaceholderText(
        QStringLiteral("例如：6.0"));

    auto* formLayout = new QFormLayout;
    formLayout->addRow(
        QStringLiteral("运动编号："), idEdit_);
    formLayout->addRow(
        QStringLiteral("运动名称："), nameEdit_);
    formLayout->addRow(
        QStringLiteral("MET值："), metEdit_);
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(addButton_);
    buttonLayout->addWidget(deleteButton_);
    formLayout->addRow(buttonLayout);

    layout->addLayout(formLayout);
    layout->addWidget(exerciseTable_);
    exerciseTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    exerciseTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(addButton_,
            &QPushButton::clicked,
            this,
            &ExercisePage::addExercise);
    connect(deleteButton_,
            &QPushButton::clicked,
            this,
            &ExercisePage::deleteSelectedExercise);
}

void ExercisePage::setRepository(IExerciseRepository* repository)
{
    repository_ = repository;
    refreshTable();
}

void ExercisePage::refreshTable()
{
    exerciseTable_->setRowCount(0);
    if (!repository_) {
        return;
    }

    const auto result = repository_->findAll();
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("读取失败"),
                             result.message);
        return;
    }

    exerciseTable_->setRowCount(result.data.size());
    for (qsizetype row = 0; row < result.data.size(); ++row) {
        const Exercise& exercise = result.data.at(row);
        exerciseTable_->setItem(
            row, 0, new QTableWidgetItem(exercise.id));
        exerciseTable_->setItem(
            row, 1, new QTableWidgetItem(exercise.name));
        exerciseTable_->setItem(
            row, 2, new QTableWidgetItem(
                        QString::number(exercise.metValue, 'f', 1)));
    }
}
void ExercisePage::addExercise()
{
    if (!repository_) {
        QMessageBox::warning(
            this,
            QStringLiteral("新增失败"),
            QStringLiteral("运动仓库尚未连接"));
        return;
    }

    const QString id = idEdit_->text().trimmed();
    const QString name = nameEdit_->text().trimmed();

    bool metOk = false;
    const double metValue =
        metEdit_->text().trimmed().toDouble(&metOk);

    if (id.isEmpty() || name.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("输入错误"),
            QStringLiteral("编号和运动名称不能为空"));
        return;
    }

    if (!metOk || metValue <= 0.0) {
        QMessageBox::warning(
            this,
            QStringLiteral("输入错误"),
            QStringLiteral("MET值必须是大于0的数字"));
        return;
    }

    Exercise exercise;
    exercise.id = id;
    exercise.name = name;
    exercise.metValue = metValue;

    const auto result = repository_->add(exercise);

    if (!result.ok) {
        QMessageBox::warning(
            this,
            QStringLiteral("新增失败"),
            result.message);
        return;
    }

    idEdit_->clear();
    nameEdit_->clear();
    metEdit_->clear();

    refreshTable();

    QMessageBox::information(
        this,
        QStringLiteral("新增成功"),
        QStringLiteral("运动数据已经保存到SQLite"));
}

void ExercisePage::deleteSelectedExercise()
{
    if (!repository_) {
        QMessageBox::warning(
            this,
            QStringLiteral("删除失败"),
            QStringLiteral("运动仓库尚未连接"));
        return;
    }

    const int row = exerciseTable_->currentRow();
    if (row < 0 || !exerciseTable_->item(row, 0)) {
        QMessageBox::information(
            this,
            QStringLiteral("未选择数据"),
            QStringLiteral("请先在表格中选中一项运动"));
        return;
    }

    const QString id = exerciseTable_->item(row, 0)->text();
    const QString name = exerciseTable_->item(row, 1)->text();
    if (QMessageBox::question(
            this,
            QStringLiteral("确认删除"),
            QStringLiteral("确定删除运动“%1”（%2）吗？")
                .arg(name, id)) != QMessageBox::Yes) {
        return;
    }

    const auto result = repository_->remove(id);
    if (!result.ok) {
        QMessageBox::warning(this,
                             QStringLiteral("删除失败"),
                             result.message);
        return;
    }
    if (!result.data) {
        QMessageBox::information(this,
                                 QStringLiteral("未删除"),
                                 QStringLiteral("该运动已不存在"));
        refreshTable();
        return;
    }

    refreshTable();
    QMessageBox::information(this,
                             QStringLiteral("删除成功"),
                             QStringLiteral("运动已从SQLite删除"));
}
