#include "recommendation/MealRecommender.h"

#include <QMap>
#include <QRandomGenerator>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

namespace {

constexpr double kMaximumAllowedToleranceRatio = 0.10;
constexpr double kComparisonEpsilon = 1e-9;
constexpr int kMaximumSupportedItemsPerMeal = 2;
constexpr int kRandomSingleRecipePoolSize = 3;
constexpr int kRandomPlanPoolSize = 5;
constexpr int kMaximumChoicesPerMeal = 240;
constexpr int kMaximumChoicesPerCalorieBucket = 24;
constexpr int kMealPlanBeamWidth = 240;
constexpr double kMealChoiceCalorieBucketSize = 50.0;

bool isFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool isFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool isValidPreference(const RecommendationPreference& preference)
{
    for (auto it = preference.itemWeights.cbegin();
         it != preference.itemWeights.cend();
         ++it) {
        if (it.key().trimmed().isEmpty()
            || !isFiniteNonNegative(it.value())) {
            return false;
        }
    }
    for (auto it = preference.keywordWeights.cbegin();
         it != preference.keywordWeights.cend();
         ++it) {
        if (it.key().trimmed().isEmpty() || !std::isfinite(it.value())) {
            return false;
        }
    }
    return true;
}

QString normalizedKeyword(const QString& value)
{
    return value.trimmed().toLower();
}

QHash<QString, double> buildRecipePreferenceScores(
    const QVector<Recipe>& recipes,
    const RecommendationPreference& preference)
{
    QHash<QString, double> scores;
    if (!preference.hasSignals()) {
        return scores;
    }

    QHash<QString, int> documentFrequencies;
    for (const Recipe& recipe : recipes) {
        QSet<QString> uniqueTags;
        for (const QString& tag : recipe.nutritionTags) {
            const QString normalized = normalizedKeyword(tag);
            if (!normalized.isEmpty()) {
                uniqueTags.insert(normalized);
            }
        }
        for (const QString& tag : uniqueTags) {
            documentFrequencies[tag] += 1;
        }
    }

    const double documentCount = static_cast<double>(recipes.size());
    QHash<QString, double> inverseDocumentFrequencies;
    for (auto it = documentFrequencies.cbegin();
         it != documentFrequencies.cend();
         ++it) {
        inverseDocumentFrequencies.insert(
            it.key(),
            std::log((documentCount + 1.0)
                     / (static_cast<double>(it.value()) + 1.0))
                + 1.0);
    }

    QHash<QString, double> userVector;
    double userNormSquared = 0.0;
    for (auto it = preference.keywordWeights.cbegin();
         it != preference.keywordWeights.cend();
         ++it) {
        const QString keyword = normalizedKeyword(it.key());
        if (!inverseDocumentFrequencies.contains(keyword)) {
            continue;
        }
        const double value = it.value()
            * inverseDocumentFrequencies.value(keyword);
        userVector.insert(keyword, value);
        userNormSquared += value * value;
    }

    for (const Recipe& recipe : recipes) {
        const double itemScore =
            preference.itemWeights.value(recipe.id, 1.0) - 1.0;
        QSet<QString> uniqueTags;
        for (const QString& tag : recipe.nutritionTags) {
            const QString normalized = normalizedKeyword(tag);
            if (!normalized.isEmpty()) {
                uniqueTags.insert(normalized);
            }
        }

        double tagSimilarity = 0.0;
        if (!uniqueTags.isEmpty() && userNormSquared > kComparisonEpsilon) {
            const double termFrequency = 1.0 / uniqueTags.size();
            double dotProduct = 0.0;
            double recipeNormSquared = 0.0;
            for (const QString& tag : uniqueTags) {
                const double recipeValue = termFrequency
                    * inverseDocumentFrequencies.value(tag, 0.0);
                recipeNormSquared += recipeValue * recipeValue;
                dotProduct += recipeValue * userVector.value(tag, 0.0);
            }
            if (recipeNormSquared > kComparisonEpsilon) {
                tagSimilarity = dotProduct
                    / (std::sqrt(recipeNormSquared)
                       * std::sqrt(userNormSquared));
            }
        }

        // 单项目星级每颗星造成 0.2 的真实分差；标签相似度作为较温和的
        // 辅助信号，避免压过明确的项目反馈。
        scores.insert(recipe.id, itemScore + 0.4 * tagSimilarity);
    }
    return scores;
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
//只能够exclude吗？
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
    double mealTargetCalories,
    QRandomGenerator* randomGenerator)
{
    if (randomGenerator != nullptr) {
        QVector<const Recipe*> rankedCandidates;
        rankedCandidates.reserve(candidates.size());
        for (const Recipe& recipe : candidates) {
            rankedCandidates.append(&recipe);
        }

        // 先按原有质量规则排序，再只在最接近目标的少量候选中随机选择，
        // 既增加菜单变化，也避免随机到明显偏离餐次目标的食谱。
        std::stable_sort(
            rankedCandidates.begin(),
            rankedCandidates.end(),
            [mealTargetCalories](const Recipe* left, const Recipe* right) {
                const double leftDifference =
                    std::abs(left->totalCalories - mealTargetCalories);
                const double rightDifference =
                    std::abs(right->totalCalories - mealTargetCalories);
                if (std::abs(leftDifference - rightDifference)
                    > kComparisonEpsilon) {
                    return leftDifference < rightDifference;
                }
                return left->totalCalories
                    < right->totalCalories - kComparisonEpsilon;
            });

        const int poolSize = std::min(
            kRandomSingleRecipePoolSize,
            static_cast<int>(rankedCandidates.size()));
        const int selectedIndex = randomGenerator->bounded(poolSize);
        return makeMealPlanItem(*rankedCandidates.at(selectedIndex));
    }

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
    double dailyTargetCalories,
    QRandomGenerator* randomGenerator)
{
    if (mealRatio <= kComparisonEpsilon) {
        return;
    }

    const std::optional<MealPlanItem> selected =
        selectClosestSingleRecipe(
            candidates,
            dailyTargetCalories * mealRatio,
            randomGenerator);
    if (selected.has_value()) {
        destination.append(*selected);
    }
}

struct MealChoice {
    MealType mealType = MealType::Breakfast;
    const Recipe* firstRecipe = nullptr;
    const Recipe* secondRecipe = nullptr;
    int itemCount = 0;
    double calories = 0.0;
    NutritionFacts nutrition;
    double targetRatio = 0.0;
    double ratioDifference = 0.0;
    double macroDifference = 0.0;
    double preferenceScore = 0.0;
};

double macroDifference(
    const NutritionFacts& actual,
    const NutritionFacts& target);

NutritionFacts scaledNutritionTarget(
    const NutritionFacts& target,
    double ratio)
{
    NutritionFacts scaled;
    scaled.caloriesKcal = target.caloriesKcal * ratio;
    scaled.proteinG = target.proteinG * ratio;
    scaled.carbohydrateG = target.carbohydrateG * ratio;
    scaled.fatG = target.fatG * ratio;
    return scaled;
}

bool isBetterMealChoice(
    const MealChoice& candidate,
    const MealChoice& current)
{
    if (std::abs(candidate.ratioDifference - current.ratioDifference)
        > kComparisonEpsilon) {
        return candidate.ratioDifference < current.ratioDifference;
    }
    if (std::abs(candidate.macroDifference - current.macroDifference)
        > kComparisonEpsilon) {
        return candidate.macroDifference < current.macroDifference;
    }
    if (std::abs(candidate.preferenceScore - current.preferenceScore)
        > kComparisonEpsilon) {
        return candidate.preferenceScore > current.preferenceScore;
    }
    if (candidate.itemCount != current.itemCount) {
        return candidate.itemCount < current.itemCount;
    }
    return candidate.calories
        < current.calories - kComparisonEpsilon;
}

QVector<MealChoice> buildMealChoices(
    const QVector<Recipe>& candidates,
    MealType mealType,
    int maximumItemsPerMeal,
    double targetCalories,
    double mealRatio,
    double maximumDailyCalories,
    const std::optional<NutritionFacts>& nutritionTarget,
    const QHash<QString, double>& recipePreferenceScores)
{
    // 先按热量区间保留每个餐次的代表性优质候选，避免某一个热量点
    // 占满候选池，同时把后续跨餐次搜索控制在固定规模内。
    QMap<int, QVector<MealChoice>> choicesByCalorieBucket;
    const double mealTargetCalories = targetCalories * mealRatio;
    const std::optional<NutritionFacts> mealNutritionTarget =
        nutritionTarget.has_value()
        ? std::optional<NutritionFacts>(
              scaledNutritionTarget(*nutritionTarget, mealRatio))
        : std::nullopt;

    const auto appendChoice = [&](const Recipe& first,
                                  const Recipe* second) {
        MealChoice choice;
        choice.mealType = mealType;
        choice.firstRecipe = &first;
        choice.secondRecipe = second;
        choice.itemCount = second == nullptr ? 1 : 2;
        choice.targetRatio = mealRatio;
        choice.calories = first.totalCalories
            + (second == nullptr ? 0.0 : second->totalCalories);
        if (choice.calories
            > maximumDailyCalories + kComparisonEpsilon) {
            return;
        }

        choice.nutrition = normalizedNutrition(first);
        if (second != nullptr) {
            addNutrition(choice.nutrition, normalizedNutrition(*second));
        }
        choice.ratioDifference = std::abs(
            choice.calories - mealTargetCalories);
        choice.macroDifference = mealNutritionTarget.has_value()
            ? macroDifference(choice.nutrition, *mealNutritionTarget)
            : 0.0;
        const double firstPreference = recipePreferenceScores.value(
            first.id,
            0.0);
        const double secondPreference = second == nullptr
            ? 0.0
            : recipePreferenceScores.value(second->id, 0.0);
        choice.preferenceScore =
            (firstPreference + secondPreference) / choice.itemCount;

        const int bucket = static_cast<int>(std::floor(
            choice.calories / kMealChoiceCalorieBucketSize));
        choicesByCalorieBucket[bucket].append(std::move(choice));
    };

    for (const Recipe& recipe : candidates) {
        appendChoice(recipe, nullptr);
    }

    if (maximumItemsPerMeal >= 2) {
        for (qsizetype i = 0; i < candidates.size() - 1; ++i) {
            for (qsizetype j = i + 1; j < candidates.size(); ++j) {
                appendChoice(candidates.at(i), &candidates.at(j));
            }
        }
    }

    QVector<MealChoice> choices;
    for (auto it = choicesByCalorieBucket.begin();
         it != choicesByCalorieBucket.end();
         ++it) {
        QVector<MealChoice>& bucketChoices = it.value();
        std::stable_sort(
            bucketChoices.begin(),
            bucketChoices.end(),
            isBetterMealChoice);
        if (bucketChoices.size() > kMaximumChoicesPerCalorieBucket) {
            bucketChoices.resize(kMaximumChoicesPerCalorieBucket);
        }
        choices += std::move(bucketChoices);
    }

    std::stable_sort(
        choices.begin(),
        choices.end(),
        isBetterMealChoice);
    if (choices.size() > kMaximumChoicesPerMeal) {
        choices.resize(kMaximumChoicesPerMeal);
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

void appendChoiceToPlan(MealPlan& plan, const MealChoice& choice)
{
    QVector<MealPlanItem> items;
    items.reserve(choice.itemCount);
    items.append(makeMealPlanItem(*choice.firstRecipe));
    if (choice.secondRecipe != nullptr) {
        items.append(makeMealPlanItem(*choice.secondRecipe));
    }

    switch (choice.mealType) {
    case MealType::Breakfast:
        plan.breakfast = std::move(items);
        break;
    case MealType::Lunch:
        plan.lunch = std::move(items);
        break;
    case MealType::Dinner:
        plan.dinner = std::move(items);
        break;
    case MealType::Snack:
        plan.snacks = std::move(items);
        break;
    }
    plan.totalCalories += choice.calories;
    addNutrition(plan.totalNutrition, choice.nutrition);
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

struct ScoredMealPlan {
    MealPlan plan;
    double ratioDifference = 0.0;
    double macroDifference = 0.0;
    double preferenceScore = 0.0;
    double dailyDifference = 0.0;
};

bool isBetterPlanScore(
    const ScoredMealPlan& candidate,
    const ScoredMealPlan& current)
{
    return candidate.ratioDifference
            < current.ratioDifference - kComparisonEpsilon
        || (std::abs(candidate.ratioDifference - current.ratioDifference)
                <= kComparisonEpsilon
            && (candidate.macroDifference
                    < current.macroDifference - kComparisonEpsilon
                || (std::abs(candidate.macroDifference
                                - current.macroDifference)
                        <= kComparisonEpsilon
                    && (candidate.preferenceScore
                            > current.preferenceScore + kComparisonEpsilon
                        || (std::abs(candidate.preferenceScore
                                        - current.preferenceScore)
                                <= kComparisonEpsilon
                            && (candidate.dailyDifference
                                    < current.dailyDifference
                                        - kComparisonEpsilon
                                || (std::abs(candidate.dailyDifference
                                                - current.dailyDifference)
                                        <= kComparisonEpsilon
                                    && mealPlanItemCount(candidate.plan)
                                        < mealPlanItemCount(
                                            current.plan))))))));
}

std::optional<MealPlan> findBestMultiRecipePlan(
    const RecipesByMealType& groupedRecipes,
    double targetCalories,
    const MealRecommendationOptions& options,
    const QHash<QString, double>& recipePreferenceScores,
    QRandomGenerator* randomGenerator)
{
    QVector<QVector<MealChoice>> choicesByEnabledMeal;

    const double minimumCalories =
        targetCalories * (1.0 - options.toleranceRatio);
    const double maximumCalories =
        targetCalories * (1.0 + options.toleranceRatio);

    const auto appendChoicesIfEnabled = [
                                            &choicesByEnabledMeal,
                                            &options,
                                            &recipePreferenceScores,
                                            targetCalories,
                                            maximumCalories](
                                            const QVector<Recipe>& candidates,
                                            MealType mealType,
                                            double ratio) {
        if (ratio > kComparisonEpsilon) {
            choicesByEnabledMeal.append(buildMealChoices(
                candidates,
                mealType,
                options.maximumItemsPerMeal,
                targetCalories,
                ratio,
                maximumCalories,
                options.nutritionTarget,
                recipePreferenceScores));
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

    for (const QVector<MealChoice>& choices : choicesByEnabledMeal) {
        if (choices.isEmpty()) {
            return std::nullopt;
        }
    }

    const int mealCount = static_cast<int>(choicesByEnabledMeal.size());
    QVector<double> remainingMinimumCalories(mealCount + 1, 0.0);
    QVector<double> remainingMaximumCalories(mealCount + 1, 0.0);
    for (int mealIndex = mealCount - 1; mealIndex >= 0; --mealIndex) {
        double minimumChoiceCalories =
            std::numeric_limits<double>::infinity();
        double maximumChoiceCalories = 0.0;
        for (const MealChoice& choice :
             choicesByEnabledMeal.at(mealIndex)) {
            minimumChoiceCalories = std::min(
                minimumChoiceCalories,
                choice.calories);
            maximumChoiceCalories = std::max(
                maximumChoiceCalories,
                choice.calories);
        }
        remainingMinimumCalories[mealIndex] =
            minimumChoiceCalories
            + remainingMinimumCalories.at(mealIndex + 1);
        remainingMaximumCalories[mealIndex] =
            maximumChoiceCalories
            + remainingMaximumCalories.at(mealIndex + 1);
    }

    struct PartialMealPlan {
        QVector<const MealChoice*> choices;
        double totalCalories = 0.0;
        NutritionFacts totalNutrition;
        double ratioDifference = 0.0;
        double preferenceTotal = 0.0;
        int itemCount = 0;
        double processedRatio = 0.0;
    };

    const auto preferenceScoreOf = [](const PartialMealPlan& plan) {
        return plan.itemCount == 0
            ? 0.0
            : plan.preferenceTotal / plan.itemCount;
    };
    const auto partialMacroDifference = [&options](
                                            const PartialMealPlan& plan) {
        return options.nutritionTarget.has_value()
            ? macroDifference(
                  plan.totalNutrition,
                  scaledNutritionTarget(
                      *options.nutritionTarget,
                      plan.processedRatio))
            : 0.0;
    };
    const auto isBetterPartial = [
                                     targetCalories,
                                     &partialMacroDifference,
                                     &preferenceScoreOf](
                                     const PartialMealPlan& candidate,
                                     const PartialMealPlan& current) {
        const double candidateMacro = partialMacroDifference(candidate);
        const double currentMacro = partialMacroDifference(current);
        const double candidatePreference = preferenceScoreOf(candidate);
        const double currentPreference = preferenceScoreOf(current);
        const double candidateProgressDifference = std::abs(
            candidate.totalCalories
            - targetCalories * candidate.processedRatio);
        const double currentProgressDifference = std::abs(
            current.totalCalories
            - targetCalories * current.processedRatio);

        if (std::abs(candidate.ratioDifference
                     - current.ratioDifference)
            > kComparisonEpsilon) {
            return candidate.ratioDifference < current.ratioDifference;
        }
        if (std::abs(candidateMacro - currentMacro)
            > kComparisonEpsilon) {
            return candidateMacro < currentMacro;
        }
        if (std::abs(candidatePreference - currentPreference)
            > kComparisonEpsilon) {
            return candidatePreference > currentPreference;
        }
        if (std::abs(candidateProgressDifference
                     - currentProgressDifference)
            > kComparisonEpsilon) {
            return candidateProgressDifference
                < currentProgressDifference;
        }
        return candidate.itemCount < current.itemCount;
    };

    QVector<PartialMealPlan> beam(1);
    for (int mealIndex = 0; mealIndex < mealCount; ++mealIndex) {
        QVector<PartialMealPlan> expanded;
        expanded.reserve(
            beam.size()
            * choicesByEnabledMeal.at(mealIndex).size());
        const int nextMealIndex = mealIndex + 1;

        for (const PartialMealPlan& partial : beam) {
            for (const MealChoice& choice :
                 choicesByEnabledMeal.at(mealIndex)) {
                const double nextCalories =
                    partial.totalCalories + choice.calories;
                if (nextCalories
                        + remainingMinimumCalories.at(nextMealIndex)
                    > maximumCalories + kComparisonEpsilon) {
                    continue;
                }
                if (nextCalories
                        + remainingMaximumCalories.at(nextMealIndex)
                        + kComparisonEpsilon
                    < minimumCalories) {
                    continue;
                }

                PartialMealPlan candidate = partial;
                candidate.choices.append(&choice);
                candidate.totalCalories = nextCalories;
                addNutrition(candidate.totalNutrition, choice.nutrition);
                candidate.ratioDifference += choice.ratioDifference;
                candidate.preferenceTotal +=
                    choice.preferenceScore * choice.itemCount;
                candidate.itemCount += choice.itemCount;
                candidate.processedRatio += choice.targetRatio;
                expanded.append(std::move(candidate));
            }
        }

        if (expanded.isEmpty()) {
            return std::nullopt;
        }
        std::stable_sort(
            expanded.begin(),
            expanded.end(),
            isBetterPartial);
        if (expanded.size() > kMealPlanBeamWidth) {
            expanded.resize(kMealPlanBeamWidth);
        }
        beam = std::move(expanded);
    }

    QVector<ScoredMealPlan> rankedPlans;
    rankedPlans.reserve(beam.size());
    for (const PartialMealPlan& partial : beam) {
        if (partial.totalCalories + kComparisonEpsilon < minimumCalories
            || partial.totalCalories
                > maximumCalories + kComparisonEpsilon) {
            continue;
        }

        MealPlan plan;
        for (const MealChoice* choice : partial.choices) {
            appendChoiceToPlan(plan, *choice);
        }
        rankedPlans.append({
            std::move(plan),
            partial.ratioDifference,
            options.nutritionTarget.has_value()
                ? macroDifference(
                      partial.totalNutrition,
                      *options.nutritionTarget)
                : 0.0,
            preferenceScoreOf(partial),
            std::abs(partial.totalCalories - targetCalories)});
    }

    if (rankedPlans.isEmpty()) {
        return std::nullopt;
    }
    std::stable_sort(
        rankedPlans.begin(),
        rankedPlans.end(),
        isBetterPlanScore);

    if (randomGenerator != nullptr) {
        const int poolSize = std::min(
            kRandomPlanPoolSize,
            static_cast<int>(rankedPlans.size()));
        const int selectedIndex = randomGenerator->bounded(poolSize);
        return std::move(rankedPlans[selectedIndex].plan);
    }
    return std::move(rankedPlans.first().plan);
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
        || invalidNutritionTarget
        || !isValidPreference(options.preference)) {
        return ServiceResult<MealPlan>::failure(
            QStringLiteral("INVALID_OPTIONS"),
            QStringLiteral(
                "食谱推荐选项不合法：容差必须为 0～10%，各餐比例必须为"
                "非负有限数且合计为 1，加餐开关必须与加餐比例一致，"
                "每餐最大项目数必须为 1～2；营养目标中的蛋白质、碳水和"
                "脂肪必须为非负有限数，且至少一项大于 0；反馈偏好中的"
                "项目权重必须为非负有限数，关键词权重必须为有限数。"));
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
    const QHash<QString, double> recipePreferenceScores =
        buildRecipePreferenceScores(
            eligibleRecipes,
            options.preference);
    const QStringList missingMealTypes = findMissingRequiredMealTypes(
        groupedRecipes,
        options);

    if (!missingMealTypes.isEmpty()) {
        return ServiceResult<MealPlan>::failure(
            QStringLiteral("MISSING_MEAL_TYPE_CANDIDATES"),
            QStringLiteral("以下启用餐次没有候选食谱：%1。")
                .arg(missingMealTypes.join(QStringLiteral("、"))));
    }

    std::optional<QRandomGenerator> randomGenerator;
    if (options.randomSeed.has_value()) {
        randomGenerator.emplace(*options.randomSeed);
    }
    QRandomGenerator* generator = randomGenerator.has_value()
        ? &*randomGenerator
        : nullptr;

    MealPlan singleRecipePlan;
    appendSingleMealIfEnabled(
        singleRecipePlan.breakfast,
        groupedRecipes.breakfast,
        options.breakfastRatio,
        targetCalories,
        generator);
    appendSingleMealIfEnabled(
        singleRecipePlan.lunch,
        groupedRecipes.lunch,
        options.lunchRatio,
        targetCalories,
        generator);
    appendSingleMealIfEnabled(
        singleRecipePlan.dinner,
        groupedRecipes.dinner,
        options.dinnerRatio,
        targetCalories,
        generator);
    appendSingleMealIfEnabled(
        singleRecipePlan.snacks,
        groupedRecipes.snacks,
        options.snackRatio,
        targetCalories,
        generator);

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
        && !options.nutritionTarget.has_value()
        && !options.preference.hasSignals()) {
        return ServiceResult<MealPlan>::success(
            std::move(singleRecipePlan),
            QStringLiteral("已生成每餐一份食谱的每日膳食计划。"));
    }

    const std::optional<MealPlan> bestMultiRecipePlan =
        findBestMultiRecipePlan(
            groupedRecipes,
            targetCalories,
            options,
            recipePreferenceScores,
            generator);

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
