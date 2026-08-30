#include "recommendation/MealRecommender.h"

#include <QSet>

#include <cmath>
#include <functional>
#include <optional>
#include <utility>

namespace {

constexpr double kMaximumAllowedToleranceRatio = 0.10;
constexpr double kComparisonEpsilon = 1e-9;
constexpr int kMaximumSupportedItemsPerMeal = 2;

bool isFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool isFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

double normalizedNutritionValue(double value)
{
    return isFiniteNonNegative(value) ? value : 0.0;
}

NutritionFacts normalizedNutrition(const Recipe& recipe)
{
    NutritionFacts nutrition = recipe.nutritionPerServing;
    // The legacy calorie field remains authoritative until all imported data
    // consistently provides the detailed nutrition object.
    nutrition.caloriesKcal = recipe.totalCalories;
    nutrition.proteinG = normalizedNutritionValue(nutrition.proteinG);
    nutrition.carbohydrateG = normalizedNutritionValue(nutrition.carbohydrateG);
    nutrition.fatG = normalizedNutritionValue(nutrition.fatG);
    nutrition.saturatedFatG = normalizedNutritionValue(nutrition.saturatedFatG);
    nutrition.fiberG = normalizedNutritionValue(nutrition.fiberG);
    nutrition.sugarG = normalizedNutritionValue(nutrition.sugarG);
    nutrition.sodiumMg = normalizedNutritionValue(nutrition.sodiumMg);
    nutrition.cholesterolMg = normalizedNutritionValue(nutrition.cholesterolMg);
    return nutrition;
}

void addNutrition(NutritionFacts& total, const NutritionFacts& value)
{
    total.caloriesKcal += value.caloriesKcal;
    total.proteinG += value.proteinG;
    total.carbohydrateG += value.carbohydrateG;
    total.fatG += value.fatG;
    total.saturatedFatG += value.saturatedFatG;
    total.fiberG += value.fiberG;
    total.sugarG += value.sugarG;
    total.sodiumMg += value.sodiumMg;
    total.cholesterolMg += value.cholesterolMg;
}

QVector<Recipe> filterEligibleRecipes(
    const UserProfile& user,
    const QVector<Recipe>& recipeDatabase,
    const MealRecommendationOptions& options)
{
    QSet<QString> excludedIds;
    for (const QString& id : user.dislikedRecipeIds) {
        excludedIds.insert(id.trimmed());
    }
    for (const QString& id : options.excludedRecipeIds) {
        excludedIds.insert(id.trimmed());
    }

    QVector<Recipe> eligibleRecipes;
    eligibleRecipes.reserve(recipeDatabase.size());
    QSet<QString> acceptedIds;

    for (const Recipe& recipe : recipeDatabase) {
        const QString normalizedId = recipe.id.trimmed();
        const bool hasValidId = !normalizedId.isEmpty();
        const bool hasValidCalories = isFinitePositive(recipe.totalCalories);
        const bool isExcluded = excludedIds.contains(normalizedId);
        const bool isDuplicate = acceptedIds.contains(normalizedId);
        const bool isDisabledSnack =
            recipe.mealType == MealType::Snack && !options.includeSnack;

        if (!hasValidId
            || !hasValidCalories
            || isExcluded
            || isDuplicate
            || isDisabledSnack) {
            continue;
        }

        Recipe normalizedRecipe = recipe;
        normalizedRecipe.id = normalizedId;
        eligibleRecipes.append(std::move(normalizedRecipe));
        acceptedIds.insert(normalizedId);
    }

    return eligibleRecipes;
}

struct RecipesByMealType {
    QVector<Recipe> breakfast;
    QVector<Recipe> lunch;
    QVector<Recipe> dinner;
    QVector<Recipe> snacks;
};

RecipesByMealType groupRecipesByMealType(
    const QVector<Recipe>& eligibleRecipes)
{
    RecipesByMealType grouped;

    for (const Recipe& recipe : eligibleRecipes) {
        switch (recipe.mealType) {
        case MealType::Breakfast:
            grouped.breakfast.append(recipe);
            break;
        case MealType::Lunch:
            grouped.lunch.append(recipe);
            break;
        case MealType::Dinner:
            grouped.dinner.append(recipe);
            break;
        case MealType::Snack:
            grouped.snacks.append(recipe);
            break;
        }
    }

    return grouped;
}

QStringList findMissingRequiredMealTypes(
    const RecipesByMealType& groupedRecipes,
    const MealRecommendationOptions& options)
{
    QStringList missingMealTypes;

    // 比例为 0 表示本次计划不启用该餐次，因此不要求数据库提供候选。
    if (options.breakfastRatio > kComparisonEpsilon
        && groupedRecipes.breakfast.isEmpty()) {
        missingMealTypes.append(QStringLiteral("早餐"));
    }
    if (options.lunchRatio > kComparisonEpsilon
        && groupedRecipes.lunch.isEmpty()) {
        missingMealTypes.append(QStringLiteral("午餐"));
    }
    if (options.dinnerRatio > kComparisonEpsilon
        && groupedRecipes.dinner.isEmpty()) {
        missingMealTypes.append(QStringLiteral("晚餐"));
    }
    if (options.includeSnack
        && options.snackRatio > kComparisonEpsilon
        && groupedRecipes.snacks.isEmpty()) {
        missingMealTypes.append(QStringLiteral("加餐"));
    }

    return missingMealTypes;
}

MealPlanItem makeMealPlanItem(const Recipe& recipe)
{
    MealPlanItem item;
    item.recipeId = recipe.id;
    item.recipeName = recipe.name;
    item.mealType = recipe.mealType;
    item.ingredients = recipe.ingredients;
    item.nutritionTags = recipe.nutritionTags;
    item.calories = recipe.totalCalories;
    item.nutrition = normalizedNutrition(recipe);
    return item;
}

std::optional<MealPlanItem> selectClosestSingleRecipe(
    const QVector<Recipe>& candidates,
    double mealTargetCalories)
{
    std::optional<MealPlanItem> bestItem;
    double bestDifference = 0.0;

    for (const Recipe& recipe : candidates) {
        const double difference =
            std::abs(recipe.totalCalories - mealTargetCalories);

        // 热量偏差相同时选择热量较低的食谱，避免无必要地增加摄入。
        const bool isBetter =
            !bestItem.has_value()
            || difference < bestDifference - kComparisonEpsilon
            || (std::abs(difference - bestDifference) <= kComparisonEpsilon
                && recipe.totalCalories
                    < bestItem->calories - kComparisonEpsilon);

        if (isBetter) {
            bestItem = makeMealPlanItem(recipe);
            bestDifference = difference;
        }
    }

    return bestItem;
}

void appendSingleMealIfEnabled(
    QVector<MealPlanItem>& destination,
    const QVector<Recipe>& candidates,
    double mealRatio,
    double dailyTargetCalories)
{
    if (mealRatio <= kComparisonEpsilon) {
        return;
    }

    const std::optional<MealPlanItem> selected =
        selectClosestSingleRecipe(
            candidates,
            dailyTargetCalories * mealRatio);
    if (selected.has_value()) {
        destination.append(*selected);
    }
}

struct MealChoice {
    MealType mealType = MealType::Breakfast;
    QVector<MealPlanItem> items;
    double calories = 0.0;
};

QVector<MealChoice> buildMealChoices(
    const QVector<Recipe>& candidates,
    MealType mealType,
    int maximumItemsPerMeal)
{
    QVector<MealChoice> choices;

    for (const Recipe& recipe : candidates) {
        const MealPlanItem item = makeMealPlanItem(recipe);
        choices.append({mealType, {item}, item.calories});
    }

    if (maximumItemsPerMeal >= 2) {
        for (qsizetype i = 0; i < candidates.size() - 1; ++i) {
            for (qsizetype j = i + 1; j < candidates.size(); ++j) {
                const MealPlanItem firstItem =
                    makeMealPlanItem(candidates.at(i));
                const MealPlanItem secondItem =
                    makeMealPlanItem(candidates.at(j));
                choices.append({
                    mealType,
                    {firstItem, secondItem},
                    firstItem.calories + secondItem.calories});
            }
        }
    }

    return choices;
}

int mealPlanItemCount(const MealPlan& plan)
{
    return plan.breakfast.size()
        + plan.lunch.size()
        + plan.dinner.size()
        + plan.snacks.size();
}

double mealRatioDifference(
    const QVector<MealChoice>& choices,
    double targetCalories,
    const MealRecommendationOptions& options)
{
    double totalDifference = 0.0;

    for (const MealChoice& choice : choices) {
        double ratio = 0.0;
        switch (choice.mealType) {
        case MealType::Breakfast:
            ratio = options.breakfastRatio;
            break;
        case MealType::Lunch:
            ratio = options.lunchRatio;
            break;
        case MealType::Dinner:
            ratio = options.dinnerRatio;
            break;
        case MealType::Snack:
            ratio = options.snackRatio;
            break;
        }

        totalDifference += std::abs(
            choice.calories - targetCalories * ratio);
    }

    return totalDifference;
}

void appendChoiceToPlan(MealPlan& plan, const MealChoice& choice)
{
    switch (choice.mealType) {
    case MealType::Breakfast:
        plan.breakfast = choice.items;
        break;
    case MealType::Lunch:
        plan.lunch = choice.items;
        break;
    case MealType::Dinner:
        plan.dinner = choice.items;
        break;
    case MealType::Snack:
        plan.snacks = choice.items;
        break;
    }
    plan.totalCalories += choice.calories;
    for (const MealPlanItem& item : choice.items) {
        addNutrition(plan.totalNutrition, item.nutrition);
    }
}

double macroDifference(
    const NutritionFacts& actual,
    const NutritionFacts& target)
{
    double totalRelativeDifference = 0.0;
    int activeTargets = 0;

    const auto addDifference = [&](double actualValue, double targetValue) {
        // 值为 0 表示调用方没有为该营养素设置目标，不参与评分。
        if (targetValue > kComparisonEpsilon) {
            totalRelativeDifference +=
                std::abs(actualValue - targetValue) / targetValue;
            ++activeTargets;
        }
    };

    addDifference(actual.proteinG, target.proteinG);
    addDifference(actual.carbohydrateG, target.carbohydrateG);
    addDifference(actual.fatG, target.fatG);

    return activeTargets == 0
        ? 0.0
        : totalRelativeDifference / activeTargets;
}

std::optional<MealPlan> findBestMultiRecipePlan(
    const RecipesByMealType& groupedRecipes,
    double targetCalories,
    const MealRecommendationOptions& options)
{
    QVector<QVector<MealChoice>> choicesByEnabledMeal;

    const auto appendChoicesIfEnabled = [&choicesByEnabledMeal, &options](
                                            const QVector<Recipe>& candidates,
                                            MealType mealType,
                                            double ratio) {
        if (ratio > kComparisonEpsilon) {
            choicesByEnabledMeal.append(buildMealChoices(
                candidates,
                mealType,
                options.maximumItemsPerMeal));
        }
    };

    appendChoicesIfEnabled(
        groupedRecipes.breakfast,
        MealType::Breakfast,
        options.breakfastRatio);
    appendChoicesIfEnabled(
        groupedRecipes.lunch,
        MealType::Lunch,
        options.lunchRatio);
    appendChoicesIfEnabled(
        groupedRecipes.dinner,
        MealType::Dinner,
        options.dinnerRatio);
    appendChoicesIfEnabled(
        groupedRecipes.snacks,
        MealType::Snack,
        options.snackRatio);

    const double minimumCalories =
        targetCalories * (1.0 - options.toleranceRatio);
    const double maximumCalories =
        targetCalories * (1.0 + options.toleranceRatio);
    std::optional<MealPlan> bestPlan;
    double bestRatioDifference = 0.0;
    double bestMacroDifference = 0.0;
    double bestDailyDifference = 0.0;
    QVector<MealChoice> currentChoices;

    std::function<void(qsizetype, double)> search =
        [&](qsizetype mealIndex, double currentCalories) {
            // 所有食谱热量均为正数，超过上限后无需继续向下搜索。
            if (currentCalories > maximumCalories + kComparisonEpsilon) {
                return;
            }

            if (mealIndex == choicesByEnabledMeal.size()) {
                if (currentCalories + kComparisonEpsilon < minimumCalories) {
                    return;
                }

                MealPlan candidate;
                for (const MealChoice& choice : currentChoices) {
                    appendChoiceToPlan(candidate, choice);
                }

                const double ratioDifference = mealRatioDifference(
                    currentChoices,
                    targetCalories,
                    options);
                const double dailyDifference =
                    std::abs(candidate.totalCalories - targetCalories);
                const double candidateMacroDifference =
                    options.nutritionTarget.has_value()
                    ? macroDifference(
                          candidate.totalNutrition,
                          *options.nutritionTarget)
                    : 0.0;
                const bool isBetter =
                    !bestPlan.has_value()
                    || ratioDifference
                        < bestRatioDifference - kComparisonEpsilon
                    || (std::abs(ratioDifference - bestRatioDifference)
                            <= kComparisonEpsilon
                        && (candidateMacroDifference
                                < bestMacroDifference - kComparisonEpsilon
                            || (std::abs(candidateMacroDifference
                                            - bestMacroDifference)
                                    <= kComparisonEpsilon
                                && (dailyDifference
                                        < bestDailyDifference
                                            - kComparisonEpsilon
                                    || (std::abs(dailyDifference
                                                    - bestDailyDifference)
                                            <= kComparisonEpsilon
                                        && mealPlanItemCount(candidate)
                                            < mealPlanItemCount(
                                                *bestPlan))))));

                if (isBetter) {
                    bestPlan = std::move(candidate);
                    bestRatioDifference = ratioDifference;
                    bestMacroDifference = candidateMacroDifference;
                    bestDailyDifference = dailyDifference;
                }
                return;
            }

            for (const MealChoice& choice :
                 choicesByEnabledMeal.at(mealIndex)) {
                currentChoices.append(choice);
                search(mealIndex + 1, currentCalories + choice.calories);
                currentChoices.removeLast();
            }
        };

    search(0, 0.0);
    return bestPlan;
}

} // namespace

ServiceResult<MealPlan> MealRecommender::generate(
    const UserProfile& user,
    double targetCalories,
    const QVector<Recipe>& recipeDatabase,
    const MealRecommendationOptions& options) const
{
    if (!isFinitePositive(targetCalories)) {
        return ServiceResult<MealPlan>::failure(
            QStringLiteral("INVALID_TARGET"),
            QStringLiteral("每日目标摄入热量必须是大于 0 的有限数值。"));
    }

    if (recipeDatabase.isEmpty()) {
        return ServiceResult<MealPlan>::failure(
            QStringLiteral("EMPTY_RECIPE_DATABASE"),
            QStringLiteral("食谱数据库为空，无法生成每日食谱。"));
    }

    const bool invalidTolerance =
        !std::isfinite(options.toleranceRatio)
        || options.toleranceRatio < 0.0
        || options.toleranceRatio
            > kMaximumAllowedToleranceRatio + kComparisonEpsilon;

    const bool invalidRatios =
        !isFiniteNonNegative(options.breakfastRatio)
        || !isFiniteNonNegative(options.lunchRatio)
        || !isFiniteNonNegative(options.dinnerRatio)
        || !isFiniteNonNegative(options.snackRatio);

    const double ratioTotal =
        options.breakfastRatio
        + options.lunchRatio
        + options.dinnerRatio
        + options.snackRatio;
    const bool invalidRatioTotal =
        !std::isfinite(ratioTotal)
        || std::abs(ratioTotal - 1.0) > kComparisonEpsilon;

    // includeSnack 与 snackRatio 必须表达一致意图：
    // 启用加餐时比例应大于 0，禁用时比例必须为 0。
    const bool invalidSnackOptions = options.includeSnack
        ? options.snackRatio <= 0.0
        : options.snackRatio > kComparisonEpsilon;

    const bool invalidItemCount =
        options.maximumItemsPerMeal <= 0
        || options.maximumItemsPerMeal > kMaximumSupportedItemsPerMeal;

    bool invalidNutritionTarget = false;
    if (options.nutritionTarget.has_value()) {
        const NutritionFacts& target = *options.nutritionTarget;
        invalidNutritionTarget =
            !isFiniteNonNegative(target.proteinG)
            || !isFiniteNonNegative(target.carbohydrateG)
            || !isFiniteNonNegative(target.fatG)
            || (target.proteinG <= kComparisonEpsilon
                && target.carbohydrateG <= kComparisonEpsilon
                && target.fatG <= kComparisonEpsilon);
    }

    if (invalidTolerance
        || invalidRatios
        || invalidRatioTotal
        || invalidSnackOptions
        || invalidItemCount
        || invalidNutritionTarget) {
        return ServiceResult<MealPlan>::failure(
            QStringLiteral("INVALID_OPTIONS"),
            QStringLiteral(
                "食谱推荐选项不合法：容差必须为 0～10%，各餐比例必须为"
                "非负有限数且合计为 1，加餐开关必须与加餐比例一致，"
                "每餐最大项目数必须为 1～2；营养目标中的蛋白质、碳水和"
                "脂肪必须为非负有限数，且至少一项大于 0。"));
    }

    const QVector<Recipe> eligibleRecipes = filterEligibleRecipes(
        user,
        recipeDatabase,
        options);

    if (eligibleRecipes.isEmpty()) {
        return ServiceResult<MealPlan>::failure(
            QStringLiteral("NO_ELIGIBLE_RECIPE"),
            QStringLiteral(
                "过滤无效、重复、不喜欢、显式排除及禁用加餐食谱后，"
                "没有可推荐项目。"));
    }

    const RecipesByMealType groupedRecipes =
        groupRecipesByMealType(eligibleRecipes);
    const QStringList missingMealTypes = findMissingRequiredMealTypes(
        groupedRecipes,
        options);

    if (!missingMealTypes.isEmpty()) {
        return ServiceResult<MealPlan>::failure(
            QStringLiteral("MISSING_MEAL_TYPE_CANDIDATES"),
            QStringLiteral("以下启用餐次没有候选食谱：%1。")
                .arg(missingMealTypes.join(QStringLiteral("、"))));
    }

    MealPlan singleRecipePlan;
    appendSingleMealIfEnabled(
        singleRecipePlan.breakfast,
        groupedRecipes.breakfast,
        options.breakfastRatio,
        targetCalories);
    appendSingleMealIfEnabled(
        singleRecipePlan.lunch,
        groupedRecipes.lunch,
        options.lunchRatio,
        targetCalories);
    appendSingleMealIfEnabled(
        singleRecipePlan.dinner,
        groupedRecipes.dinner,
        options.dinnerRatio,
        targetCalories);
    appendSingleMealIfEnabled(
        singleRecipePlan.snacks,
        groupedRecipes.snacks,
        options.snackRatio,
        targetCalories);

    const auto addMealNutrition = [&singleRecipePlan](
                                      const QVector<MealPlanItem>& items) {
        for (const MealPlanItem& item : items) {
            singleRecipePlan.totalCalories += item.calories;
            addNutrition(singleRecipePlan.totalNutrition, item.nutrition);
        }
    };
    addMealNutrition(singleRecipePlan.breakfast);
    addMealNutrition(singleRecipePlan.lunch);
    addMealNutrition(singleRecipePlan.dinner);
    addMealNutrition(singleRecipePlan.snacks);

    const double minimumDailyCalories =
        targetCalories * (1.0 - options.toleranceRatio);
    const double maximumDailyCalories =
        targetCalories * (1.0 + options.toleranceRatio);
    const bool dailyCaloriesWithinTolerance =
        singleRecipePlan.totalCalories + kComparisonEpsilon
            >= minimumDailyCalories
        && singleRecipePlan.totalCalories
            <= maximumDailyCalories + kComparisonEpsilon;

    if (dailyCaloriesWithinTolerance
        && !options.nutritionTarget.has_value()) {
        return ServiceResult<MealPlan>::success(
            std::move(singleRecipePlan),
            QStringLiteral("已生成每餐一份食谱的每日膳食计划。"));
    }

    const std::optional<MealPlan> bestMultiRecipePlan =
        findBestMultiRecipePlan(
            groupedRecipes,
            targetCalories,
            options);

    if (bestMultiRecipePlan.has_value()) {
        return ServiceResult<MealPlan>::success(
            *bestMultiRecipePlan,
            QStringLiteral("已生成满足全天热量范围的多食谱膳食计划。"));
    }

    // 所有允许的每餐 1～2 份组合均已搜索完毕，仍不存在合法解。
    return ServiceResult<MealPlan>::failure(
        QStringLiteral("NO_FEASIBLE_MEAL_PLAN"),
        QStringLiteral(
            "在每餐项目数和全天热量容差限制内找不到可行膳食计划。"));
}
