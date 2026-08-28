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
    user.activityLevel = 3;
    user.goalType = GoalType::Lose;
    user.weeklyGoalKg = 0.5;
    user.dietContributionRatio = 0.7;

    const auto result = calculator.calculate(user);
    if (!result.ok) return 1;
    if (!approximatelyEqual(result.data.bmi, 26.1224)) return 2;
    if (!approximatelyEqual(result.data.bmr, 1773.75)) return 3;
    if (!approximatelyEqual(result.data.tdee, 2749.3125)) return 4;
    if (!approximatelyEqual(result.data.dailyDeficit, 550.0)) return 5;
    if (!approximatelyEqual(result.data.dietDeficit, 385.0)) return 6;
    if (!approximatelyEqual(result.data.recommendedIntake, 2364.3125)) return 7;
    if (!approximatelyEqual(result.data.exerciseTarget, 165.0)) return 8;

    user.heightCm = 0.0;
    if (calculator.calculate(user).ok) return 9;

    return 0;
}
