#include "recommendation/WeeklyPlanner.h"

#include <cmath>
#include <limits>

int main()
{
    // 这里先验证公共周计划选项的默认契约。
    // WeeklyPlanner 实现完成后，再增加七天计划的集成测试。
    const WeeklyPlanOptions options;

    if (options.numberOfDays != 7) {
        return 1;
    }

    if (!options.avoidConsecutiveDuplicateExercises) {
        return 2;
    }

    if (!options.avoidConsecutiveDuplicateRecipes) {
        return 3;
    }

    if (std::abs(options.dailyTargetVariationRatio - 0.10) > 1e-9) {
        return 26;
    }

    // 既有测试聚焦原来的固定目标路径；波动行为在文件末尾单独验证。
    WeeklyPlanOptions fixedTargetOptions = options;
    fixedTargetOptions.dailyTargetVariationRatio = 0.0;

    WeeklyPlanner planner;

    UserProfile user;
    user.id = QStringLiteral("user-1");
    user.weightKg = 70.0;

    CalorieNeed calorieNeed;
    calorieNeed.recommendedIntake = 1800.0;
    calorieNeed.exerciseTarget = 196.0;

    Exercise exercise;
    exercise.id = QStringLiteral("running");
    exercise.name = QStringLiteral("跑步");
    exercise.metValue = 8.0;

    Recipe recipe;
    recipe.id = QStringLiteral("breakfast-1");
    recipe.name = QStringLiteral("早餐");
    recipe.totalCalories = 500.0;
    recipe.mealType = MealType::Breakfast;

    Recipe lunchRecipe;
    lunchRecipe.id = QStringLiteral("lunch-1");
    lunchRecipe.name = QStringLiteral("午餐");
    lunchRecipe.totalCalories = 650.0;
    lunchRecipe.mealType = MealType::Lunch;

    Recipe dinnerRecipe;
    dinnerRecipe.id = QStringLiteral("dinner-1");
    dinnerRecipe.name = QStringLiteral("晚餐");
    dinnerRecipe.totalCalories = 550.0;
    dinnerRecipe.mealType = MealType::Dinner;

    const QVector<Recipe> recipes{recipe, lunchRecipe, dinnerRecipe};

    const QDate validStartDate(2026, 8, 31);

    UserProfile missingIdUser = user;
    missingIdUser.id = QStringLiteral("   ");
    if (planner.generate(
            missingIdUser,
            calorieNeed,
            validStartDate,
            {exercise},
            recipes,
            fixedTargetOptions).code != QStringLiteral("INVALID_USER")) {
        return 4;
    }

    UserProfile invalidWeightUser = user;
    invalidWeightUser.weightKg = 0.0;
    if (planner.generate(
            invalidWeightUser,
            calorieNeed,
            validStartDate,
            {exercise},
            recipes,
            fixedTargetOptions).code != QStringLiteral("INVALID_USER")) {
        return 5;
    }

    CalorieNeed invalidCalorieNeed = calorieNeed;
    invalidCalorieNeed.recommendedIntake =
        std::numeric_limits<double>::infinity();
    if (planner.generate(
            user,
            invalidCalorieNeed,
            validStartDate,
            {exercise},
            recipes,
            fixedTargetOptions).code != QStringLiteral("INVALID_CALORIE_NEED")) {
        return 6;
    }

    if (planner.generate(
            user,
            calorieNeed,
            {},
            {exercise},
            recipes,
            fixedTargetOptions).code != QStringLiteral("INVALID_START_DATE")) {
        return 7;
    }

    WeeklyPlanOptions invalidDays = fixedTargetOptions;
    invalidDays.numberOfDays = 6;
    if (planner.generate(
            user,
            calorieNeed,
            validStartDate,
            {exercise},
            recipes,
            invalidDays).code != QStringLiteral("INVALID_OPTIONS")) {
        return 8;
    }

    if (planner.generate(
            user,
            calorieNeed,
            validStartDate,
            {},
            recipes,
            fixedTargetOptions).code != QStringLiteral("EMPTY_EXERCISE_DATABASE")) {
        return 9;
    }

    if (planner.generate(
            user,
            calorieNeed,
            validStartDate,
            {exercise},
            {},
            fixedTargetOptions).code != QStringLiteral("EMPTY_RECIPE_DATABASE")) {
        return 10;
    }

    const auto validInputResult = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {exercise},
        recipes,
        fixedTargetOptions);
    if (!validInputResult.ok
        || validInputResult.code != QStringLiteral("OK")
        || validInputResult.data.days.size() != 7
        || validInputResult.data.schemaVersion != QStringLiteral("1.0")
        || validInputResult.data.planId.trimmed().isEmpty()
        || validInputResult.data.userId != user.id
        || validInputResult.data.startDate != validStartDate
        || !validInputResult.data.generatedAt.isValid()) {
        return 11;
    }

    double calculatedWeeklyCaloriesIn = 0.0;
    double calculatedWeeklyCaloriesOut = 0.0;
    for (int dayIndex = 0;
         dayIndex < validInputResult.data.days.size();
         ++dayIndex) {
        const DailyPlan& day = validInputResult.data.days.at(dayIndex);
        if (day.date != validStartDate.addDays(dayIndex)
            || day.completed
            || day.exercises.isEmpty()
            || day.meals.breakfast.isEmpty()
            || day.meals.lunch.isEmpty()
            || day.meals.dinner.isEmpty()
            || std::abs(day.totalCaloriesBurned - 196.0) > 1e-9) {
            return 12;
        }
        calculatedWeeklyCaloriesIn += day.meals.totalCalories;
        calculatedWeeklyCaloriesOut += day.totalCaloriesBurned;
    }

    if (std::abs(validInputResult.data.totalCaloriesIn
                 - calculatedWeeklyCaloriesIn) > 1e-9
        || std::abs(validInputResult.data.totalCaloriesOut
                    - calculatedWeeklyCaloriesOut) > 1e-9) {
        return 13;
    }

    // 当前数据库每个餐次和运动都只有一个候选，因此第 2～7 天必须
    // 回退为允许重复，并通过 warnings 告知调用方。
    if (validInputResult.warnings.size() != 6) {
        return 14;
    }

    Exercise invalidExercise = exercise;
    invalidExercise.metValue = 0.0;
    const auto exerciseFailureResult = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {invalidExercise},
        recipes,
        fixedTargetOptions);
    if (exerciseFailureResult.code
        != QStringLiteral("EXERCISE_RECOMMENDATION_FAILED")
        || !exerciseFailureResult.message.contains(
            QStringLiteral("NO_ELIGIBLE_EXERCISE"))) {
        return 15;
    }

    const auto mealFailureResult = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {exercise},
        {recipe},
        fixedTargetOptions);
    if (mealFailureResult.code
        != QStringLiteral("MEAL_RECOMMENDATION_FAILED")
        || !mealFailureResult.message.contains(
            QStringLiteral("MISSING_MEAL_TYPE_CANDIDATES"))) {
        return 16;
    }

    Exercise cycling = exercise;
    cycling.id = QStringLiteral("cycling");
    cycling.name = QStringLiteral("骑行");
    cycling.metValue = 4.0;

    Recipe secondBreakfast = recipe;
    secondBreakfast.id = QStringLiteral("breakfast-2");
    secondBreakfast.totalCalories = 520.0;
    Recipe secondLunch = lunchRecipe;
    secondLunch.id = QStringLiteral("lunch-2");
    secondLunch.totalCalories = 640.0;
    Recipe secondDinner = dinnerRecipe;
    secondDinner.id = QStringLiteral("dinner-2");
    secondDinner.totalCalories = 560.0;

    const auto diversityResult = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {exercise, cycling},
        {recipe,
         secondBreakfast,
         lunchRecipe,
         secondLunch,
         dinnerRecipe,
         secondDinner},
        fixedTargetOptions);

    if (!diversityResult.ok || !diversityResult.warnings.isEmpty()) {
        return 17;
    }

    for (int dayIndex = 1;
         dayIndex < diversityResult.data.days.size();
         ++dayIndex) {
        const DailyPlan& previous = diversityResult.data.days.at(dayIndex - 1);
        const DailyPlan& current = diversityResult.data.days.at(dayIndex);

        if (previous.exercises.first().exerciseId
            == current.exercises.first().exerciseId) {
            return 18;
        }

        const bool repeatedBreakfast =
            previous.meals.breakfast.first().recipeId
            == current.meals.breakfast.first().recipeId;
        const bool repeatedLunch =
            previous.meals.lunch.first().recipeId
            == current.meals.lunch.first().recipeId;
        const bool repeatedDinner =
            previous.meals.dinner.first().recipeId
            == current.meals.dinner.first().recipeId;
        if (repeatedBreakfast || repeatedLunch || repeatedDinner) {
            return 19;
        }
    }

    WeeklyPlanOptions duplicatesAllowed = fixedTargetOptions;
    duplicatesAllowed.avoidConsecutiveDuplicateExercises = false;
    duplicatesAllowed.avoidConsecutiveDuplicateRecipes = false;
    const auto duplicatesAllowedResult = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {exercise},
        recipes,
        duplicatesAllowed);
    if (!duplicatesAllowedResult.ok
        || !duplicatesAllowedResult.warnings.isEmpty()) {
        return 20;
    }

    // 食谱只有一套、运动有替代项时，应只放宽食谱去重，继续轮换运动。
    const auto partialFallbackResult = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {exercise, cycling},
        recipes,
        fixedTargetOptions);
    if (!partialFallbackResult.ok
        || partialFallbackResult.warnings.size() != 6) {
        return 21;
    }
    for (int dayIndex = 1;
         dayIndex < partialFallbackResult.data.days.size();
         ++dayIndex) {
        if (partialFallbackResult.data.days.at(dayIndex - 1)
                .exercises.first().exerciseId
            == partialFallbackResult.data.days.at(dayIndex)
                .exercises.first().exerciseId) {
            return 22;
        }
    }

    CalorieNeed nanExerciseTarget = calorieNeed;
    nanExerciseTarget.exerciseTarget =
        std::numeric_limits<double>::quiet_NaN();
    if (planner.generate(
            user,
            nanExerciseTarget,
            validStartDate,
            {exercise},
            recipes,
            fixedTargetOptions).code != QStringLiteral("INVALID_CALORIE_NEED")) {
        return 23;
    }

    // 最大日期无法再覆盖后续六天，应在生成前直接拒绝。
    if (planner.generate(
            user,
            calorieNeed,
            QDate::fromJulianDay(std::numeric_limits<qint64>::max()),
            {exercise},
            recipes,
            fixedTargetOptions).code != QStringLiteral("INVALID_START_DATE")) {
        return 24;
    }

    // 每次生成都应获得独立计划 ID，生成时间使用 UTC。
    const auto secondGeneration = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {exercise},
        recipes,
        fixedTargetOptions);
    if (!secondGeneration.ok
        || secondGeneration.data.planId == validInputResult.data.planId
        || validInputResult.data.generatedAt.timeSpec() != Qt::UTC
        || secondGeneration.data.generatedAt.timeSpec() != Qt::UTC) {
        return 25;
    }

    WeeklyPlanOptions variationOptions = fixedTargetOptions;
    variationOptions.dailyTargetVariationRatio = 0.03;
    variationOptions.randomSeed = 42;
    variationOptions.avoidConsecutiveDuplicateExercises = false;
    variationOptions.avoidConsecutiveDuplicateRecipes = false;
    variationOptions.exerciseOptions.durationStepMinutes = 1;

    const auto variationResult = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {exercise},
        recipes,
        variationOptions);
    const auto repeatedVariationResult = planner.generate(
        user,
        calorieNeed,
        validStartDate,
        {exercise},
        recipes,
        variationOptions);

    if (!variationResult.ok || !repeatedVariationResult.ok) {
        return 27;
    }

    double weeklyIntakeTarget = 0.0;
    double weeklyExerciseTarget = 0.0;
    for (int dayIndex = 0; dayIndex < 7; ++dayIndex) {
        const CalorieNeed& varied =
            variationResult.data.days.at(dayIndex).calorieNeed;
        const CalorieNeed& repeated =
            repeatedVariationResult.data.days.at(dayIndex).calorieNeed;
        weeklyIntakeTarget += varied.recommendedIntake;
        weeklyExerciseTarget += varied.exerciseTarget;

        if (std::abs(varied.recommendedIntake
                     - repeated.recommendedIntake) > 1e-9
            || std::abs(varied.exerciseTarget
                        - repeated.exerciseTarget) > 1e-9
            || varied.recommendedIntake
                < calorieNeed.recommendedIntake * 0.97 - 1e-9
            || varied.recommendedIntake
                > calorieNeed.recommendedIntake * 1.03 + 1e-9) {
            return 28;
        }

        if (dayIndex > 0
            && std::abs(varied.recommendedIntake
                        - variationResult.data.days.at(dayIndex - 1)
                              .calorieNeed.recommendedIntake) <= 1e-9) {
            return 29;
        }
    }

    if (std::abs(weeklyIntakeTarget
                 - calorieNeed.recommendedIntake * 7.0) > 1e-9
        || std::abs(weeklyExerciseTarget
                    - calorieNeed.exerciseTarget * 7.0) > 1e-9) {
        return 30;
    }

    WeeklyPlanOptions invalidVariation = fixedTargetOptions;
    invalidVariation.dailyTargetVariationRatio = 0.11;
    if (planner.generate(
            user,
            calorieNeed,
            validStartDate,
            {exercise},
            recipes,
            invalidVariation).code != QStringLiteral("INVALID_OPTIONS")) {
        return 31;
    }

    return 0;
}
