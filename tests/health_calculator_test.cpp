#include "recommendation/healthcalculator.h"

#include <QtGlobal>

namespace {

bool approximatelyEqual(double left, double right, double tolerance = 0.01)
{
    return qAbs(left - right) <= tolerance;
}

} // namespace

int main()
{
    HealthCalculator calculator;

    UserProfile user;
    user.gender = Gender::Male;
    user.age = 25;
    user.heightCm = 175.0;
    user.weightKg = 80.0;
    user.averageDailySteps = 8000;
    user.goalType = GoalType::Lose;
    user.weeklyGoalKg = 0.5;
    user.dietContributionRatio = 0.7;

    const auto result = calculator.calculate(user);
    if (!result.ok) return 1;
    if (!approximatelyEqual(result.data.bmi, 26.1224)) return 2;
    if (!approximatelyEqual(result.data.bmr, 1773.75)) return 3;
    if (!approximatelyEqual(result.data.tdee, 2483.25)) return 4;
    if (!approximatelyEqual(result.data.dailyDeficit, 550.0)) return 5;
    if (!approximatelyEqual(result.data.dietDeficit, 385.0)) return 6;
    if (!approximatelyEqual(result.data.recommendedIntake, 2098.25)) return 7;
    if (!approximatelyEqual(result.data.exerciseTarget, 165.0)) return 8;
    if (result.data.bmiEvaluation != QStringLiteral("Overweight")) return 10;

    UserProfile normalBmiUser = user;
    normalBmiUser.heightCm = 170.0;
    normalBmiUser.weightKg = 60.0;
    const auto normalBmiResult = calculator.calculate(normalBmiUser);
    if (!normalBmiResult.ok
        || normalBmiResult.data.bmiEvaluation
            != QStringLiteral("Normal weight")
        || !normalBmiResult.warnings.join(QLatin1Char('|')).contains(
            QStringLiteral("normal range"))) {
        return 11;
    }

    UserProfile underweightUser = normalBmiUser;
    underweightUser.weightKg = 45.0;
    const auto underweightResult = calculator.calculate(underweightUser);
    if (!underweightResult.ok
        || underweightResult.data.bmiEvaluation
            != QStringLiteral("Underweight")
        || !underweightResult.warnings.join(QLatin1Char('|')).contains(
            QStringLiteral("not recommended"))) {
        return 12;
    }

    UserProfile obesityBoundaryUser = user;
    obesityBoundaryUser.heightCm = 175.0;
    obesityBoundaryUser.weightKg = 85.75;
    const auto obesityBoundaryResult = calculator.calculate(
        obesityBoundaryUser);
    if (!obesityBoundaryResult.ok
        || obesityBoundaryResult.data.bmiEvaluation
            != QStringLiteral("Obesity")) {
        return 13;
    }

    struct StepCase {
        int steps;
        double factor;
    };
    const StepCase stepCases[] = {
        {0, 1.20}, {4999, 1.20},
        {5000, 1.30}, {7499, 1.30},
        {7500, 1.40}, {9999, 1.40},
        {10000, 1.50}, {12499, 1.50},
        {12500, 1.60}, {50000, 1.60}
    };
    for (const StepCase& stepCase : stepCases) {
        user.averageDailySteps = stepCase.steps;
        const auto stepResult = calculator.calculate(user);
        if (!stepResult.ok
            || !approximatelyEqual(stepResult.data.tdee,
                                   result.data.bmr * stepCase.factor)) {
            return 9;
        }
    }

    user.averageDailySteps = -1;
    if (calculator.calculate(user).ok) return 10;
    user.averageDailySteps = 50001;
    if (calculator.calculate(user).ok) return 11;

    user.averageDailySteps = 8000;
    user.heightCm = 0.0;
    if (calculator.calculate(user).ok) return 12;

    return 0;
}
