#pragma once

#include "models/NutritionFacts.h"

#include <QWidget>

class MacronutrientChart : public QWidget
{
public:
    explicit MacronutrientChart(QWidget* parent = nullptr);

    void setNutrition(const NutritionFacts& nutrition);
    void clearNutrition();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    NutritionFacts nutrition_;
    bool hasNutrition_ = false;
};
