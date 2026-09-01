#include "recommendation/healthcalculator.h"

#include <QStringList>
#include <algorithm>

namespace {

constexpr double caloriesPerKilogram = 7700.0;

double activityFactor(int activityLevel)
{
    switch (activityLevel) {
    case 1: return 1.2;
    case 2: return 1.375;
    case 3: return 1.55;
    case 4: return 1.725;
    case 5: return 1.9;
    default: return 0.0;
    }
}

QString bmiEvaluation(double bmi)
{
    if (bmi < 18.5) return QStringLiteral("Underweight");
    if (bmi < 24.0) return QStringLiteral("Normal weight");
    if (bmi < 28.0) return QStringLiteral("Overweight");
    return QStringLiteral("Obesity");
}

} // namespace

ServiceResult<CalorieNeed> HealthCalculator::calculate(
    const UserProfile& user) const
{
    if (user.age < 18 || user.heightCm <= 0.0 || user.weightKg <= 0.0) {
        return ServiceResult<CalorieNeed>::failure(
            QStringLiteral("INVALID_USER_PROFILE"),
            QStringLiteral(
                "Age must be at least 18, and height and weight must be positive."));
    }

    if (user.activityLevel < 1 || user.activityLevel > 5) {
        return ServiceResult<CalorieNeed>::failure(
            QStringLiteral("INVALID_ACTIVITY_LEVEL"),
            QStringLiteral("Activity level must be between 1 and 5."));
    }

    if (user.dietContributionRatio < 0.0
        || user.dietContributionRatio > 1.0) {
        return ServiceResult<CalorieNeed>::failure(
            QStringLiteral("INVALID_DIET_RATIO"),
            QStringLiteral("Diet contribution ratio must be between 0 and 1."));
    }

    if (user.goalType != GoalType::Maintain && user.weeklyGoalKg <= 0.0) {
        return ServiceResult<CalorieNeed>::failure(
            QStringLiteral("INVALID_WEEKLY_GOAL"),
            QStringLiteral("Weekly goal must be greater than zero."));
    }

    CalorieNeed need;
    const double heightMeters = user.heightCm / 100.0;
    need.bmi = user.weightKg / (heightMeters * heightMeters);
    need.bmiEvaluation = bmiEvaluation(need.bmi);

    need.bmr = 10.0 * user.weightKg
               + 6.25 * user.heightCm
               - 5.0 * user.age
               + (user.gender == Gender::Male ? 5.0 : -161.0);
    need.tdee = need.bmr * activityFactor(user.activityLevel);

    const double dailyEnergyChange =
        user.weeklyGoalKg * caloriesPerKilogram / 7.0;

    switch (user.goalType) {
    case GoalType::Lose:
        need.dailyDeficit = dailyEnergyChange;
        need.dietDeficit = dailyEnergyChange * user.dietContributionRatio;
        need.recommendedIntake =
            std::max(0.0, need.tdee - need.dietDeficit);
        need.exerciseTarget = dailyEnergyChange - need.dietDeficit;
        break;
    case GoalType::Maintain:
        need.dailyDeficit = 0.0;
        need.dietDeficit = 0.0;
        need.recommendedIntake = need.tdee;
        need.exerciseTarget = 0.0;
        break;
    case GoalType::Gain:
        need.dailyDeficit = -dailyEnergyChange;
        need.dietDeficit =
            -dailyEnergyChange * user.dietContributionRatio;
        need.recommendedIntake = need.tdee - need.dietDeficit;
        need.exerciseTarget = 0.0;
        break;
    }

    QStringList warnings{
        QStringLiteral(
            "Calculated values are educational estimates, not medical advice.")
    };
    if (user.age > 78) {
        warnings.append(QStringLiteral(
            "The BMR equation was derived from adults aged 19 to 78."));
    }
    if (need.recommendedIntake < need.bmr) {
        warnings.append(QStringLiteral(
            "The estimated intake is below BMR and should be reviewed."));
    }
    if (user.goalType == GoalType::Lose && need.bmi < 18.5) {
        warnings.append(QStringLiteral(
            "BMI is below 18.5; an automatic weight-loss plan is not recommended."));
    } else if (user.goalType == GoalType::Lose && need.bmi < 24.0) {
        warnings.append(QStringLiteral(
            "BMI is in the normal range; review whether further weight loss is appropriate."));
    }

    if (user.goalType == GoalType::Lose && user.targetWeightKg > 0.0) {
        const double targetBmi =
            user.targetWeightKg / (heightMeters * heightMeters);
        if (targetBmi < 18.5) {
            warnings.append(QStringLiteral(
                "The target weight would result in a BMI below 18.5."));
        }
    }

    return ServiceResult<CalorieNeed>::success(need, {}, warnings);
}
