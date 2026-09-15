#include "ui/macronutrientchart.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>

#include <algorithm>
#include <cmath>

namespace {

struct MacroValue {
    QString name;
    double grams = 0.0;
    double caloriesPerGram = 0.0;
    QColor color;
};

double safeValue(double value)
{
    return std::isfinite(value) ? std::max(0.0, value) : 0.0;
}

QVector<MacroValue> macroValues(const NutritionFacts& nutrition)
{
    return {
        {QStringLiteral("蛋白质"), safeValue(nutrition.proteinG), 4.0,
         QColor(QStringLiteral("#3F8FD2"))},
        {QStringLiteral("碳水"), safeValue(nutrition.carbohydrateG), 4.0,
         QColor(QStringLiteral("#F2A93B"))},
        {QStringLiteral("脂肪"), safeValue(nutrition.fatG), 9.0,
         QColor(QStringLiteral("#4EAD72"))}
    };
}

double roundedAxisMaximum(double maximumGrams)
{
    if (maximumGrams <= 0.0) return 100.0;
    const double roughStep = maximumGrams / 4.0;
    double step = 10.0;
    if (roughStep > 50.0) step = 100.0;
    else if (roughStep > 25.0) step = 50.0;
    else if (roughStep > 10.0) step = 25.0;
    return std::ceil(maximumGrams / step) * step;
}

} // namespace

MacronutrientChart::MacronutrientChart(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(238);
}

void MacronutrientChart::setNutrition(const NutritionFacts& nutrition)
{
    nutrition_ = nutrition;
    const QVector<MacroValue> values = macroValues(nutrition_);
    hasNutrition_ = std::any_of(
        values.cbegin(), values.cend(), [](const MacroValue& value) {
            return value.grams > 0.0;
        });
    update();
}

void MacronutrientChart::clearNutrition()
{
    nutrition_ = NutritionFacts{};
    hasNutrition_ = false;
    update();
}

QSize MacronutrientChart::sizeHint() const
{
    return QSize(620, 238);
}

QSize MacronutrientChart::minimumSizeHint() const
{
    return QSize(500, 220);
}

void MacronutrientChart::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF cardRect = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(QColor(QStringLiteral("#D7E8DB")), 1.0));
    painter.setBrush(QColor(QStringLiteral("#F8FBF8")));
    painter.drawRoundedRect(cardRect, 12.0, 12.0);

    QFont titleFont = font();
    titleFont.setPointSize(11);
    titleFont.setWeight(QFont::DemiBold);
    painter.setFont(titleFont);
    painter.setPen(QColor(QStringLiteral("#176B3A")));
    painter.drawText(QRectF(18.0, 12.0, width() - 36.0, 25.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("今日三大营养素"));

    if (!hasNutrition_) {
        painter.setFont(font());
        painter.setPen(QColor(QStringLiteral("#7A877D")));
        painter.drawText(cardRect.adjusted(18.0, 38.0, -18.0, -12.0),
                         Qt::AlignCenter,
                         QStringLiteral("当前计划暂无可用的营养数据"));
        return;
    }

    const QVector<MacroValue> values = macroValues(nutrition_);
    double totalMacroCalories = 0.0;
    double maximumGrams = 0.0;
    for (const MacroValue& value : values) {
        totalMacroCalories += value.grams * value.caloriesPerGram;
        maximumGrams = std::max(maximumGrams, value.grams);
    }

    const qreal contentTop = 46.0;
    const qreal contentBottom = height() - 14.0;
    const qreal dividerX = std::clamp(width() * 0.39, 200.0, 255.0);
    painter.setPen(QPen(QColor(QStringLiteral("#E1EBE3")), 1.0));
    painter.drawLine(QPointF(dividerX, contentTop),
                     QPointF(dividerX, contentBottom));

    const qreal ringSize = std::min<qreal>(132.0, dividerX - 55.0);
    const QRectF ringRect((dividerX - ringSize) / 2.0,
                          contentTop + 4.0,
                          ringSize,
                          ringSize);
    QPen ringBackground(QColor(QStringLiteral("#E5ECE7")), 20.0,
                        Qt::SolidLine, Qt::FlatCap);
    painter.setPen(ringBackground);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(ringRect);

    int startAngle = 90 * 16;
    for (const MacroValue& value : values) {
        const double calories = value.grams * value.caloriesPerGram;
        const int spanAngle = totalMacroCalories > 0.0
            ? -qRound(360.0 * 16.0 * calories / totalMacroCalories)
            : 0;
        painter.setPen(QPen(value.color, 20.0,
                            Qt::SolidLine, Qt::FlatCap));
        painter.drawArc(ringRect, startAngle, spanAngle);
        startAngle += spanAngle;
    }

    QFont centerCaptionFont = font();
    centerCaptionFont.setPointSize(9);
    painter.setFont(centerCaptionFont);
    painter.setPen(QColor(QStringLiteral("#66736A")));
    painter.drawText(ringRect.adjusted(18.0, 31.0, -18.0, -50.0),
                     Qt::AlignCenter,
                     QStringLiteral("供能"));
    QFont centerValueFont = font();
    centerValueFont.setPointSize(13);
    centerValueFont.setWeight(QFont::DemiBold);
    painter.setFont(centerValueFont);
    painter.setPen(QColor(QStringLiteral("#176B3A")));
    painter.drawText(ringRect.adjusted(8.0, 57.0, -8.0, -25.0),
                     Qt::AlignCenter,
                     QStringLiteral("%1 kcal").arg(totalMacroCalories, 0, 'f', 0));

    QFont legendFont = font();
    legendFont.setPointSize(8);
    painter.setFont(legendFont);
    const qreal legendY = ringRect.bottom() + 15.0;
    const qreal legendColumnWidth = (dividerX - 24.0) / values.size();
    for (qsizetype index = 0; index < values.size(); ++index) {
        const MacroValue& value = values.at(index);
        const double percentage = totalMacroCalories > 0.0
            ? value.grams * value.caloriesPerGram * 100.0
                / totalMacroCalories
            : 0.0;
        const QRectF legendRect(12.0 + index * legendColumnWidth,
                                legendY,
                                legendColumnWidth,
                                39.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(value.color);
        painter.drawEllipse(QPointF(legendRect.center().x(), legendY + 5.0),
                            4.0, 4.0);
        painter.setPen(QColor(QStringLiteral("#3E4A42")));
        painter.drawText(legendRect.adjusted(0.0, 10.0, 0.0, 0.0),
                         Qt::AlignHCenter | Qt::AlignTop,
                         QStringLiteral("%1\n%2%")
                             .arg(value.name)
                             .arg(percentage, 0, 'f', 0));
    }

    const QRectF plotRect(dividerX + 47.0,
                          contentTop + 17.0,
                          width() - dividerX - 66.0,
                          contentBottom - contentTop - 48.0);
    const double axisMaximum = roundedAxisMaximum(maximumGrams);
    QFont axisFont = font();
    axisFont.setPointSize(8);
    painter.setFont(axisFont);
    for (int tick = 0; tick <= 4; ++tick) {
        const double value = axisMaximum * tick / 4.0;
        const qreal y = plotRect.bottom()
            - plotRect.height() * tick / 4.0;
        painter.setPen(QPen(QColor(QStringLiteral("#DDE7DF")), 1.0,
                            tick == 0 ? Qt::SolidLine : Qt::DashLine));
        painter.drawLine(QPointF(plotRect.left(), y),
                         QPointF(plotRect.right(), y));
        painter.setPen(QColor(QStringLiteral("#7A877D")));
        painter.drawText(QRectF(dividerX + 4.0, y - 9.0, 37.0, 18.0),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(value, 'f', 0));
    }

    const qreal slotWidth = plotRect.width() / values.size();
    const qreal barWidth = std::min<qreal>(48.0, slotWidth * 0.48);
    for (qsizetype index = 0; index < values.size(); ++index) {
        const MacroValue& value = values.at(index);
        const qreal barHeight = axisMaximum > 0.0
            ? plotRect.height() * value.grams / axisMaximum
            : 0.0;
        const qreal centerX = plotRect.left()
            + slotWidth * (index + 0.5);
        const QRectF barRect(centerX - barWidth / 2.0,
                             plotRect.bottom() - barHeight,
                             barWidth,
                             barHeight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(value.color);
        painter.drawRoundedRect(barRect, 5.0, 5.0);
        painter.setPen(QColor(QStringLiteral("#334139")));
        painter.drawText(QRectF(centerX - slotWidth / 2.0,
                                barRect.top() - 23.0,
                                slotWidth,
                                20.0),
                         Qt::AlignCenter,
                         QStringLiteral("%1 g").arg(value.grams, 0, 'f', 1));
        painter.drawText(QRectF(centerX - slotWidth / 2.0,
                                plotRect.bottom() + 5.0,
                                slotWidth,
                                20.0),
                         Qt::AlignCenter,
                         value.name);
    }
}
