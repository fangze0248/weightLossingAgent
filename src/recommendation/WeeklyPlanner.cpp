#include "recommendation/WeeklyPlanner.h"
#include "recommendation/ExerciseRecommender.h"
#include "recommendation/MealRecommender.h"

#include <QUuid>

#include <cmath>
#include <utility>

namespace {

constexpr int kRequiredNumberOfDays = 7;
constexpr double kMaximumDailyTargetVariationRatio = 0.10;
constexpr double kComparisonEpsilon = 1e-9;
constexpr double kUnderweightBmiThreshold = 18.5;
constexpr double kOverweightBmiThreshold = 24.0;
constexpr double kProteinEnergyRatio = 0.20;
constexpr double kCarbohydrateEnergyRatio = 0.50;
constexpr double kFatEnergyRatio = 0.30;
constexpr double kCaloriesPerGramProtein = 4.0;
constexpr double kCaloriesPerGramCarbohydrate = 4.0;
constexpr double kCaloriesPerGramFat = 9.0;

// 七个系数互不相同、总和为 0，因此既能制造自然波动，又不会改变周平均目标。
constexpr double kDailyVariationPattern[kRequiredNumberOfDays] = {
    -1.0, -0.65, -0.30, 0.0, 0.25, 0.70, 1.0};

bool isFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

double bmiOf(const UserProfile& user)
{
    const double heightMeters = user.heightCm / 100.0;
    return user.weightKg / (heightMeters * heightMeters);
}

NutritionFacts automaticNutritionTarget(double targetCalories)
{
    NutritionFacts target;
    target.caloriesKcal = targetCalories;
    target.proteinG =
        targetCalories * kProteinEnergyRatio / kCaloriesPerGramProtein;
    target.carbohydrateG = targetCalories
        * kCarbohydrateEnergyRatio / kCaloriesPerGramCarbohydrate;
    target.fatG = targetCalories * kFatEnergyRatio / kCaloriesPerGramFat;
    return target;
}

CalorieNeed calorieNeedForDay(
    const CalorieNeed& base,
    int dayIndex,
    const WeeklyPlanOptions& options)
{
    const int rotation = options.randomSeed.has_value()
        ? static_cast<int>(*options.randomSeed % kRequiredNumberOfDays)
        : 0;
    const int patternIndex = (dayIndex + rotation) % kRequiredNumberOfDays;
    const double multiplier = 1.0
        + options.dailyTargetVariationRatio
            * kDailyVariationPattern[patternIndex];

    CalorieNeed result = base;
    result.recommendedIntake *= multiplier;
    result.exerciseTarget *= multiplier;
    return result;
}

WeeklyPlanOptions optionsForDay(
    const WeeklyPlanOptions& base,
    int dayIndex)
{
    WeeklyPlanOptions result = base;
    if (base.randomSeed.has_value()) {
        // 每天派生不同种子；相同周种子仍可完整复现同一份七天计划。
        result.mealOptions.randomSeed =
            *base.randomSeed
            + 0x9e3779b9U * static_cast<quint32>(dayIndex + 1);
    }
    return result;
}

ServiceResult<DailyPlan> generateDailyPlan(
    const UserProfile& user,
    const CalorieNeed& calorieNeed,
    const QDate& date,
    const QVector<Exercise>& exerciseDatabase,
    const QVector<Recipe>& recipeDatabase,
    const WeeklyPlanOptions& options)
{
    ExerciseRecommender exerciseRecommender;
    const auto exerciseResult = exerciseRecommender.generate(
        user,
        calorieNeed.exerciseTarget,
        exerciseDatabase,
        options.exerciseOptions);

    if (!exerciseResult.ok) {
        return ServiceResult<DailyPlan>::failure(
            QStringLiteral("EXERCISE_RECOMMENDATION_FAILED"),
            QStringLiteral("运动推荐失败（%1）：%2")
                .arg(exerciseResult.code, exerciseResult.message),
            exerciseResult.warnings);
    }

    MealRecommender mealRecommender;
    MealRecommendationOptions mealOptions = options.mealOptions;
    if (options.autoCalculateNutritionTarget
        && !mealOptions.nutritionTarget.has_value()) {
        // 使用当天的摄入目标计算，保证七天热量波动时营养目标同步变化。
        mealOptions.nutritionTarget = automaticNutritionTarget(
            calorieNeed.recommendedIntake);
    }
    const auto mealResult = mealRecommender.generate(
        user,
        calorieNeed.recommendedIntake,
        recipeDatabase,
        mealOptions);

    if (!mealResult.ok) {
        return ServiceResult<DailyPlan>::failure(
            QStringLiteral("MEAL_RECOMMENDATION_FAILED"),
            QStringLiteral("食谱推荐失败（%1）：%2")
                .arg(mealResult.code, mealResult.message),
            mealResult.warnings);
    }

    DailyPlan day;
    day.date = date;
    day.calorieNeed = calorieNeed;
    day.exercises = exerciseResult.data;
    day.meals = mealResult.data;
    day.completed = false;

    for (const ExercisePlanItem& item : day.exercises) {
        day.totalCaloriesBurned += item.caloriesBurned;
    }

    return ServiceResult<DailyPlan>::success(
        std::move(day),
        QStringLiteral("已生成单日计划。"));
}

void appendUniqueId(QStringList& ids, const QString& id)
{
    if (!id.isEmpty() && !ids.contains(id)) {
        ids.append(id);
    }
}

QStringList exerciseIdsOf(const DailyPlan& day)
{
    QStringList ids;
    for (const ExercisePlanItem& item : day.exercises) {
        appendUniqueId(ids, item.exerciseId);
    }
    return ids;
}

QStringList recipeIdsOf(const DailyPlan& day)
{
    QStringList ids;
    const auto appendMealIds = [&ids](const QVector<MealPlanItem>& items) {
        for (const MealPlanItem& item : items) {
            appendUniqueId(ids, item.recipeId);
        }
    };

    appendMealIds(day.meals.breakfast);
    appendMealIds(day.meals.lunch);
    appendMealIds(day.meals.dinner);
    appendMealIds(day.meals.snacks);
    return ids;
}

WeeklyPlanOptions optionsAvoidingPreviousDay(
    const WeeklyPlanOptions& baseOptions,
    const DailyPlan& previousDay,
    bool avoidExercises,
    bool avoidRecipes)
{
    WeeklyPlanOptions dayOptions = baseOptions;

    if (avoidExercises) {
        for (const QString& id : exerciseIdsOf(previousDay)) {
            appendUniqueId(dayOptions.exerciseOptions.excludedExerciseIds, id);
        }
    }

    if (avoidRecipes) {
        for (const QString& id : recipeIdsOf(previousDay)) {
            appendUniqueId(dayOptions.mealOptions.excludedRecipeIds, id);
        }
    }

    return dayOptions;
}

} // namespace

ServiceResult<WeeklyPlan> WeeklyPlanner::generate(
    const UserProfile& user,
    const CalorieNeed& calorieNeed,
    const QDate& startDate,
    const QVector<Exercise>& exerciseDatabase,
    const QVector<Recipe>& recipeDatabase,
    const WeeklyPlanOptions& options) const
{
    // userId 将写入 WeeklyPlan，体重则供每日运动热量计算使用。
    if (user.id.trimmed().isEmpty()
        || !isFinitePositive(user.weightKg)
        || !isFinitePositive(user.heightCm)) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("INVALID_USER"),
            QStringLiteral(
                "用户 ID 不能为空，身高和体重必须是大于 0 的有限数值。"));
    }

    const bool usesAutomaticNutritionTarget =
        options.autoCalculateNutritionTarget
        && !options.mealOptions.nutritionTarget.has_value();
    const double currentBmi = bmiOf(user);
    if (usesAutomaticNutritionTarget
        && user.goalType == GoalType::Lose
        && currentBmi < kUnderweightBmiThreshold) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("WEIGHT_LOSS_NOT_RECOMMENDED"),
            QStringLiteral(
                "当前 BMI 低于 18.5，不适合自动生成减重饮食计划。"));
    }

    if (usesAutomaticNutritionTarget
        && user.goalType == GoalType::Lose
        && isFinitePositive(user.targetWeightKg)) {
        UserProfile targetProfile = user;
        targetProfile.weightKg = user.targetWeightKg;
        if (bmiOf(targetProfile) < kUnderweightBmiThreshold) {
            return ServiceResult<WeeklyPlan>::failure(
                QStringLiteral("UNSAFE_TARGET_WEIGHT"),
                QStringLiteral(
                    "目标体重对应的 BMI 低于 18.5，请先调整减重目标。"));
        }
    }

    // 周计划直接使用这两个由 HealthCalculator 提供的每日目标。
    if (!isFinitePositive(calorieNeed.recommendedIntake)
        || !isFinitePositive(calorieNeed.exerciseTarget)) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("INVALID_CALORIE_NEED"),
            QStringLiteral(
                "推荐摄入热量和运动目标热量必须是大于 0 的有限数值。"));
    }

    if (!startDate.isValid()
        || !startDate.addDays(kRequiredNumberOfDays - 1).isValid()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("INVALID_START_DATE"),
            QStringLiteral("周计划开始日期无效，或无法覆盖完整七天。"));
    }

    // 老师要求基础版本必须生成完整七天计划，不接受任意天数。
    const bool invalidVariationRatio =
        !std::isfinite(options.dailyTargetVariationRatio)
        || options.dailyTargetVariationRatio < 0.0
        || options.dailyTargetVariationRatio
            > kMaximumDailyTargetVariationRatio + kComparisonEpsilon;

    if (options.numberOfDays != kRequiredNumberOfDays
        || invalidVariationRatio) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("INVALID_OPTIONS"),
            QStringLiteral(
                "基础周计划的天数必须固定为 7 天，单日目标波动比例必须为"
                " 0～10% 的有限数值。"));
    }

    if (exerciseDatabase.isEmpty()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("EMPTY_EXERCISE_DATABASE"),
            QStringLiteral("运动数据库为空，无法生成周计划。"));
    }

    if (recipeDatabase.isEmpty()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("EMPTY_RECIPE_DATABASE"),
            QStringLiteral("食谱数据库为空，无法生成周计划。"));
    }

    WeeklyPlan weeklyPlan;
    weeklyPlan.planId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    weeklyPlan.userId = user.id.trimmed();
    weeklyPlan.startDate = startDate;
    weeklyPlan.generatedAt = QDateTime::currentDateTimeUtc();

    QStringList warnings;
    if (usesAutomaticNutritionTarget
        && user.goalType == GoalType::Lose
        && currentBmi < kOverweightBmiThreshold) {
        warnings.append(QStringLiteral(
            "当前 BMI 处于正常范围，继续减重前建议确认目标体重是否合理。"));
    }

    for (int dayIndex = 0; dayIndex < kRequiredNumberOfDays; ++dayIndex) {
        const QDate date = startDate.addDays(dayIndex);
        const CalorieNeed dayCalorieNeed = calorieNeedForDay(
            calorieNeed,
            dayIndex,
            options);
        const WeeklyPlanOptions baseDayOptions = optionsForDay(
            options,
            dayIndex);
        WeeklyPlanOptions dayOptions = baseDayOptions;
        if (dayIndex > 0) {
            dayOptions = optionsAvoidingPreviousDay(
                baseDayOptions,
                weeklyPlan.days.last(),
                options.avoidConsecutiveDuplicateExercises,
                options.avoidConsecutiveDuplicateRecipes);
        }

        auto dayResult = generateDailyPlan(
            user,
            dayCalorieNeed,
            date,
            exerciseDatabase,
            recipeDatabase,
            dayOptions);

        // 两种去重同时启用但无法同时满足时，先分别尝试保留其中一种，
        // 避免某一类候选不足时把另一类本可满足的去重也放弃。
        if (!dayResult.ok && dayIndex > 0
            && options.avoidConsecutiveDuplicateExercises
            && options.avoidConsecutiveDuplicateRecipes) {
            const WeeklyPlanOptions exerciseOnlyOptions =
                optionsAvoidingPreviousDay(
                    baseDayOptions,
                    weeklyPlan.days.last(),
                    true,
                    false);
            dayResult = generateDailyPlan(
                user,
                dayCalorieNeed,
                date,
                exerciseDatabase,
                recipeDatabase,
                exerciseOnlyOptions);

            if (dayResult.ok) {
                dayResult.warnings.append(
                    QStringLiteral(
                        "%1 的食谱候选不足，仅保留了运动去重。")
                        .arg(date.toString(Qt::ISODate)));
            }
        }

        if (!dayResult.ok && dayIndex > 0
            && options.avoidConsecutiveDuplicateExercises
            && options.avoidConsecutiveDuplicateRecipes) {
            const WeeklyPlanOptions recipeOnlyOptions =
                optionsAvoidingPreviousDay(
                    baseDayOptions,
                    weeklyPlan.days.last(),
                    false,
                    true);
            dayResult = generateDailyPlan(
                user,
                dayCalorieNeed,
                date,
                exerciseDatabase,
                recipeDatabase,
                recipeOnlyOptions);

            if (dayResult.ok) {
                dayResult.warnings.append(
                    QStringLiteral(
                        "%1 的运动候选不足，仅保留了食谱去重。")
                        .arg(date.toString(Qt::ISODate)));
            }
        }

        // 所有启用约束都无法满足时，最后使用原始选项重试，保证
        // 小型数据库仍能生成完整周计划。
        if (!dayResult.ok && dayIndex > 0
            && (options.avoidConsecutiveDuplicateExercises
                || options.avoidConsecutiveDuplicateRecipes)) {
            dayResult = generateDailyPlan(
                user,
                dayCalorieNeed,
                date,
                exerciseDatabase,
                recipeDatabase,
                baseDayOptions);

            if (dayResult.ok) {
                dayResult.warnings.append(
                    QStringLiteral(
                        "%1 的候选不足，未能避免与前一天重复。")
                        .arg(date.toString(Qt::ISODate)));
            }
        }

        // 小型数据库或较粗的运动时长步长可能无法满足某个波动后的目标。
        // 此时只让当天回退到基础目标，优先保证完整七天计划仍可生成。
        if (!dayResult.ok
            && options.dailyTargetVariationRatio > kComparisonEpsilon) {
            dayResult = generateDailyPlan(
                user,
                calorieNeed,
                date,
                exerciseDatabase,
                recipeDatabase,
                baseDayOptions);

            if (dayResult.ok) {
                dayResult.warnings.append(
                    QStringLiteral(
                        "%1 的候选不足，已回退到基础摄入与运动目标。")
                        .arg(date.toString(Qt::ISODate)));
            }
        }

        if (!dayResult.ok) {
            return ServiceResult<WeeklyPlan>::failure(
                dayResult.code,
                QStringLiteral("第 %1 天（%2）生成失败：%3")
                    .arg(dayIndex + 1)
                    .arg(date.toString(Qt::ISODate), dayResult.message),
                dayResult.warnings);
        }

        warnings.append(dayResult.warnings);
        weeklyPlan.totalCaloriesIn += dayResult.data.meals.totalCalories;
        weeklyPlan.totalCaloriesOut += dayResult.data.totalCaloriesBurned;
        weeklyPlan.days.append(std::move(dayResult.data));
    }

    return ServiceResult<WeeklyPlan>::success(
        std::move(weeklyPlan),
        QStringLiteral("已生成完整七天周计划。"),
        std::move(warnings));
}
