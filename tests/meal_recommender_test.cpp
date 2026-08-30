#include "recommendation/MealRecommender.h"

#include <cmath>
#include <limits>

int main()
{
    // 这里先验证公共食谱推荐选项的默认契约。
    // MealRecommender 实现完成后，再增加食谱组合业务测试。
    const MealRecommendationOptions options;

    if (std::abs(options.toleranceRatio - 0.10) > 1e-9) {
        return 1;
    }

    const double mealRatioTotal =
        options.breakfastRatio
        + options.lunchRatio
        + options.dinnerRatio
        + options.snackRatio;

    if (std::abs(mealRatioTotal - 1.0) > 1e-9) {
        return 2;
    }

    if (options.maximumItemsPerMeal != 2) {
        return 3;
    }

    if (options.includeSnack) {
        return 4;
    }

    MealRecommender recommender;

    UserProfile user;

    Recipe breakfast;
    breakfast.id = QStringLiteral("breakfast-1");
    breakfast.name = QStringLiteral("燕麦早餐");
    breakfast.totalCalories = 450.0;
    breakfast.nutritionPerServing.proteinG = 20.0;
    breakfast.nutritionPerServing.carbohydrateG = 60.0;
    breakfast.nutritionPerServing.fatG = 10.0;
    breakfast.mealType = MealType::Breakfast;
    breakfast.ingredients.append(
        {QStringLiteral("燕麦"), 50.0, QStringLiteral("克")});
    breakfast.nutritionTags.append(QStringLiteral("高纤维"));

    const QVector<Recipe> database{breakfast};

    const auto zeroTargetResult = recommender.generate(
        user,
        0.0,
        database,
        options);
    if (zeroTargetResult.code != QStringLiteral("INVALID_TARGET")) {
        return 5;
    }

    const auto infiniteTargetResult = recommender.generate(
        user,
        std::numeric_limits<double>::infinity(),
        database,
        options);
    if (infiniteTargetResult.code != QStringLiteral("INVALID_TARGET")) {
        return 6;
    }

    const auto emptyDatabaseResult = recommender.generate(
        user,
        1800.0,
        {},
        options);
    if (emptyDatabaseResult.code
        != QStringLiteral("EMPTY_RECIPE_DATABASE")) {
        return 7;
    }

    MealRecommendationOptions excessiveTolerance = options;
    excessiveTolerance.toleranceRatio = 0.11;
    if (recommender.generate(user, 1800.0, database, excessiveTolerance).code
        != QStringLiteral("INVALID_OPTIONS")) {
        return 8;
    }

    MealRecommendationOptions invalidRatioTotal = options;
    invalidRatioTotal.lunchRatio = 0.30;
    if (recommender.generate(user, 1800.0, database, invalidRatioTotal).code
        != QStringLiteral("INVALID_OPTIONS")) {
        return 9;
    }

    MealRecommendationOptions inconsistentSnack = options;
    inconsistentSnack.includeSnack = true;
    if (recommender.generate(user, 1800.0, database, inconsistentSnack).code
        != QStringLiteral("INVALID_OPTIONS")) {
        return 10;
    }

    MealRecommendationOptions invalidItemCount = options;
    invalidItemCount.maximumItemsPerMeal = 0;
    if (recommender.generate(user, 1800.0, database, invalidItemCount).code
        != QStringLiteral("INVALID_OPTIONS")) {
        return 11;
    }

    invalidItemCount.maximumItemsPerMeal = 3;
    if (recommender.generate(user, 1800.0, database, invalidItemCount).code
        != QStringLiteral("INVALID_OPTIONS")) {
        return 12;
    }

    const auto incompleteMealTypesResult = recommender.generate(
        user,
        1800.0,
        database,
        options);
    if (incompleteMealTypesResult.code
        != QStringLiteral("MISSING_MEAL_TYPE_CANDIDATES")) {
        return 13;
    }

    Recipe invalidIdRecipe = breakfast;
    invalidIdRecipe.id = QStringLiteral("   ");

    Recipe invalidCaloriesRecipe = breakfast;
    invalidCaloriesRecipe.id = QStringLiteral("invalid-calories");
    invalidCaloriesRecipe.totalCalories = 0.0;

    const auto invalidRecipeDataResult = recommender.generate(
        user,
        1800.0,
        {invalidIdRecipe, invalidCaloriesRecipe},
        options);
    if (invalidRecipeDataResult.code
        != QStringLiteral("NO_ELIGIBLE_RECIPE")) {
        return 14;
    }

    UserProfile userWithDislike = user;
    userWithDislike.dislikedRecipeIds.append(
        QStringLiteral("  breakfast-1  "));

    const auto dislikedRecipeResult = recommender.generate(
        userWithDislike,
        1800.0,
        database,
        options);
    if (dislikedRecipeResult.code
        != QStringLiteral("NO_ELIGIBLE_RECIPE")) {
        return 15;
    }

    MealRecommendationOptions excludedOptions = options;
    excludedOptions.excludedRecipeIds.append(QStringLiteral("breakfast-1"));

    const auto explicitlyExcludedResult = recommender.generate(
        user,
        1800.0,
        database,
        excludedOptions);
    if (explicitlyExcludedResult.code
        != QStringLiteral("NO_ELIGIBLE_RECIPE")) {
        return 16;
    }

    Recipe snack;
    snack.id = QStringLiteral("snack-1");
    snack.name = QStringLiteral("水果加餐");
    snack.totalCalories = 150.0;
    snack.nutritionPerServing.proteinG = 5.0;
    snack.nutritionPerServing.carbohydrateG = 25.0;
    snack.nutritionPerServing.fatG = 3.0;
    snack.mealType = MealType::Snack;

    const auto disabledSnackResult = recommender.generate(
        user,
        1800.0,
        {snack},
        options);
    if (disabledSnackResult.code
        != QStringLiteral("NO_ELIGIBLE_RECIPE")) {
        return 17;
    }

    MealRecommendationOptions enabledSnackOptions = options;
    enabledSnackOptions.includeSnack = true;
    enabledSnackOptions.breakfastRatio = 0.25;
    enabledSnackOptions.lunchRatio = 0.35;
    enabledSnackOptions.dinnerRatio = 0.30;
    enabledSnackOptions.snackRatio = 0.10;

    const auto enabledSnackResult = recommender.generate(
        user,
        1800.0,
        {snack},
        enabledSnackOptions);
    if (enabledSnackResult.code
        != QStringLiteral("MISSING_MEAL_TYPE_CANDIDATES")) {
        return 18;
    }

    // 无效项和排除项存在时，只要还剩合法候选，就应通过过滤阶段。
    const auto mixedDatabaseResult = recommender.generate(
        userWithDislike,
        1800.0,
        {breakfast, invalidCaloriesRecipe, snack},
        enabledSnackOptions);
    if (mixedDatabaseResult.code
        != QStringLiteral("MISSING_MEAL_TYPE_CANDIDATES")) {
        return 19;
    }

    Recipe lunch;
    lunch.id = QStringLiteral("lunch-1");
    lunch.name = QStringLiteral("鸡胸肉午餐");
    lunch.totalCalories = 650.0;
    lunch.nutritionPerServing.proteinG = 45.0;
    lunch.nutritionPerServing.carbohydrateG = 70.0;
    lunch.nutritionPerServing.fatG = 18.0;
    lunch.mealType = MealType::Lunch;

    Recipe dinner;
    dinner.id = QStringLiteral("dinner-1");
    dinner.name = QStringLiteral("鱼肉晚餐");
    dinner.totalCalories = 550.0;
    dinner.nutritionPerServing.proteinG = 40.0;
    dinner.nutritionPerServing.carbohydrateG = 30.0;
    dinner.nutritionPerServing.fatG = 15.0;
    dinner.mealType = MealType::Dinner;

    const auto completeMealTypesResult = recommender.generate(
        user,
        1800.0,
        {breakfast, lunch, dinner},
        options);
    if (!completeMealTypesResult.ok
        || completeMealTypesResult.data.breakfast.size() != 1
        || completeMealTypesResult.data.lunch.size() != 1
        || completeMealTypesResult.data.dinner.size() != 1
        || !completeMealTypesResult.data.snacks.isEmpty()
        || std::abs(completeMealTypesResult.data.totalCalories - 1650.0)
            > 1e-9
        || std::abs(completeMealTypesResult.data.totalNutrition.caloriesKcal
                    - 1650.0) > 1e-9
        || std::abs(completeMealTypesResult.data.totalNutrition.proteinG
                    - 105.0) > 1e-9
        || std::abs(completeMealTypesResult.data.totalNutrition.carbohydrateG
                    - 160.0) > 1e-9
        || std::abs(completeMealTypesResult.data.totalNutrition.fatG
                    - 43.0) > 1e-9) {
        return 20;
    }

    const MealPlanItem selectedBreakfast =
        completeMealTypesResult.data.breakfast.first();
    if (selectedBreakfast.recipeId != breakfast.id
        || selectedBreakfast.recipeName != breakfast.name
        || selectedBreakfast.mealType != MealType::Breakfast
        || selectedBreakfast.ingredients.size() != 1
        || selectedBreakfast.ingredients.first().name
            != QStringLiteral("燕麦")
        || selectedBreakfast.nutritionTags
            != QStringList{QStringLiteral("高纤维")}
        || std::abs(selectedBreakfast.calories - breakfast.totalCalories)
            > 1e-9
        || std::abs(selectedBreakfast.nutrition.caloriesKcal - 450.0) > 1e-9
        || std::abs(selectedBreakfast.nutrition.proteinG - 20.0) > 1e-9) {
        return 21;
    }

    const auto completeWithSnackResult = recommender.generate(
        user,
        1800.0,
        {breakfast, lunch, dinner, snack},
        enabledSnackOptions);
    if (!completeWithSnackResult.ok
        || completeWithSnackResult.data.snacks.size() != 1
        || std::abs(completeWithSnackResult.data.totalCalories - 1800.0)
            > 1e-9) {
        return 22;
    }

    // 比例为 0 的餐次不属于本次计划，不应强制要求对应候选。
    MealRecommendationOptions lunchOnlyOptions = options;
    lunchOnlyOptions.breakfastRatio = 0.0;
    lunchOnlyOptions.lunchRatio = 1.0;
    lunchOnlyOptions.dinnerRatio = 0.0;

    const auto lunchOnlyResult = recommender.generate(
        user,
        1800.0,
        {lunch},
        lunchOnlyOptions);
    if (lunchOnlyResult.code != QStringLiteral("NO_FEASIBLE_MEAL_PLAN")) {
        return 23;
    }

    // 同一餐次有多个候选时，应选择最接近该餐目标热量的食谱。
    Recipe closerBreakfast = breakfast;
    closerBreakfast.id = QStringLiteral("breakfast-closer");
    closerBreakfast.name = QStringLiteral("更接近目标的早餐");
    closerBreakfast.totalCalories = 535.0;

    const auto closestRecipeResult = recommender.generate(
        user,
        1800.0,
        {breakfast, closerBreakfast, lunch, dinner},
        options);
    if (!closestRecipeResult.ok
        || closestRecipeResult.data.breakfast.first().recipeId
            != closerBreakfast.id) {
        return 24;
    }

    // 单食谱总热量低于全天允许下限时，本阶段不能错误地返回成功。
    Recipe lowLunch = lunch;
    lowLunch.totalCalories = 200.0;
    Recipe lowDinner = dinner;
    lowDinner.totalCalories = 200.0;

    const auto belowDailyRangeResult = recommender.generate(
        user,
        1800.0,
        {breakfast, lowLunch, lowDinner},
        options);
    if (belowDailyRangeResult.code
        != QStringLiteral("NO_FEASIBLE_MEAL_PLAN")) {
        return 25;
    }

    Recipe combinationLunch = lunch;
    combinationLunch.totalCalories = 400.0;
    Recipe secondLunch = combinationLunch;
    secondLunch.id = QStringLiteral("lunch-2");
    secondLunch.name = QStringLiteral("第二份午餐");

    // 单食谱方案为 450 + 400 + 550 = 1400 千卡，低于 1620 下限；
    // 午餐选择两份后总计 1800 千卡，应生成成功方案。
    const auto twoRecipesPerMealResult = recommender.generate(
        user,
        1800.0,
        {breakfast, combinationLunch, secondLunch, dinner},
        options);

    if (!twoRecipesPerMealResult.ok
        || twoRecipesPerMealResult.data.lunch.size() != 2
        || std::abs(twoRecipesPerMealResult.data.totalCalories - 1800.0)
            > 1e-9
        || twoRecipesPerMealResult.data.lunch.at(0).recipeId
            == twoRecipesPerMealResult.data.lunch.at(1).recipeId) {
        return 26;
    }

    MealRecommendationOptions onePerMealOptions = options;
    onePerMealOptions.maximumItemsPerMeal = 1;
    const auto onePerMealResult = recommender.generate(
        user,
        1800.0,
        {breakfast, combinationLunch, secondLunch, dinner},
        onePerMealOptions);
    if (onePerMealResult.code != QStringLiteral("NO_FEASIBLE_MEAL_PLAN")) {
        return 27;
    }

    MealRecommendationOptions negativeTolerance = options;
    negativeTolerance.toleranceRatio = -0.01;
    if (recommender.generate(user, 1800.0, database, negativeTolerance).code
        != QStringLiteral("INVALID_OPTIONS")) {
        return 28;
    }

    MealRecommendationOptions nanRatio = options;
    nanRatio.breakfastRatio =
        std::numeric_limits<double>::quiet_NaN();
    if (recommender.generate(user, 1800.0, database, nanRatio).code
        != QStringLiteral("INVALID_OPTIONS")) {
        return 29;
    }

    Recipe infiniteCaloriesRecipe = breakfast;
    infiniteCaloriesRecipe.id = QStringLiteral("infinite-calories");
    infiniteCaloriesRecipe.totalCalories =
        std::numeric_limits<double>::infinity();
    Recipe nanCaloriesRecipe = breakfast;
    nanCaloriesRecipe.id = QStringLiteral("nan-calories");
    nanCaloriesRecipe.totalCalories =
        std::numeric_limits<double>::quiet_NaN();

    const auto nonFiniteCaloriesResult = recommender.generate(
        user,
        1800.0,
        {infiniteCaloriesRecipe, nanCaloriesRecipe},
        options);
    if (nonFiniteCaloriesResult.code
        != QStringLiteral("NO_ELIGIBLE_RECIPE")) {
        return 30;
    }

    // 规范化后 ID 相同的食谱只保留数据库中的第一条。
    Recipe firstDuplicate = breakfast;
    firstDuplicate.id = QStringLiteral("  duplicate-breakfast  ");
    firstDuplicate.totalCalories = 540.0;
    Recipe secondDuplicate = firstDuplicate;
    secondDuplicate.id = QStringLiteral("duplicate-breakfast");
    secondDuplicate.totalCalories = 100.0;

    const auto duplicateResult = recommender.generate(
        user,
        1800.0,
        {firstDuplicate, secondDuplicate, lunch, dinner},
        options);
    if (!duplicateResult.ok
        || duplicateResult.data.breakfast.size() != 1
        || duplicateResult.data.breakfast.first().recipeId
            != QStringLiteral("duplicate-breakfast")
        || std::abs(duplicateResult.data.breakfast.first().calories - 540.0)
            > 1e-9) {
        return 31;
    }

    // 全天热量恰好位于容差下限和上限时都应被接受。
    Recipe boundaryBreakfast = breakfast;
    boundaryBreakfast.totalCalories = 500.0;
    Recipe boundaryLunch = lunch;
    Recipe boundaryDinner = dinner;

    boundaryLunch.totalCalories = 600.0;
    boundaryDinner.totalCalories = 520.0; // 合计 1620，为下限 90%。
    const auto lowerBoundaryResult = recommender.generate(
        user,
        1800.0,
        {boundaryBreakfast, boundaryLunch, boundaryDinner},
        options);

    boundaryLunch.totalCalories = 800.0;
    boundaryDinner.totalCalories = 680.0; // 合计 1980，为上限 110%。
    const auto upperBoundaryResult = recommender.generate(
        user,
        1800.0,
        {boundaryBreakfast, boundaryLunch, boundaryDinner},
        options);

    if (!lowerBoundaryResult.ok
        || !upperBoundaryResult.ok
        || std::abs(lowerBoundaryResult.data.totalCalories - 1620.0) > 1e-9
        || std::abs(upperBoundaryResult.data.totalCalories - 1980.0) > 1e-9) {
        return 32;
    }

    // 单份食谱方案只有 700 千卡，会进入多食谱搜索。存在多套落在全天
    // 容差内的组合时，应选出精确符合 30%/40%/30% 的 300/400/300。
    Recipe ratioBreakfastFar = breakfast;
    ratioBreakfastFar.id = QStringLiteral("ratio-breakfast-far");
    ratioBreakfastFar.totalCalories = 100.0;
    Recipe ratioBreakfastExact = breakfast;
    ratioBreakfastExact.id = QStringLiteral("ratio-breakfast-exact");
    ratioBreakfastExact.totalCalories = 200.0;
    Recipe ratioLunchFar = lunch;
    ratioLunchFar.id = QStringLiteral("ratio-lunch-far");
    ratioLunchFar.totalCalories = 100.0;
    Recipe ratioLunchExact = lunch;
    ratioLunchExact.id = QStringLiteral("ratio-lunch-exact");
    ratioLunchExact.totalCalories = 300.0;
    Recipe ratioDinnerFar = dinner;
    ratioDinnerFar.id = QStringLiteral("ratio-dinner-far");
    ratioDinnerFar.totalCalories = 100.0;
    Recipe ratioDinnerExact = dinner;
    ratioDinnerExact.id = QStringLiteral("ratio-dinner-exact");
    ratioDinnerExact.totalCalories = 200.0;

    const auto ratioPriorityResult = recommender.generate(
        user,
        1000.0,
        {ratioBreakfastFar,
         ratioBreakfastExact,
         ratioLunchFar,
         ratioLunchExact,
         ratioDinnerFar,
         ratioDinnerExact},
        options);

    if (!ratioPriorityResult.ok
        || ratioPriorityResult.data.breakfast.size() != 2
        || ratioPriorityResult.data.lunch.size() != 2
        || ratioPriorityResult.data.dinner.size() != 2
        || std::abs(ratioPriorityResult.data.totalCalories - 1000.0)
            > 1e-9) {
        return 33;
    }

    return 0;
}
