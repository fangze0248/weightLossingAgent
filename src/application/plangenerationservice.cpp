#include "application/plangenerationservice.h"

#include "interfaces/IExerciseRepository.h"
#include "interfaces/IFeedbackService.h"
#include "interfaces/IHealthCalculator.h"
#include "interfaces/IPlanRepository.h"
#include "interfaces/IRecipeRepository.h"
#include "interfaces/IUserRepository.h"
#include "interfaces/IWeeklyPlanner.h"

#include <QRandomGenerator>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

// A seven-day plan can consume up to two recipes per meal and three exercise
// items per day. Keep enough SQL-ranked candidates for whole-week diversity
// without loading the complete tables into memory.
constexpr int kRecipeQueryLimitPerMeal = 256;
constexpr int kRecipeQualityCandidatesPerMeal = 32;
constexpr int kRecipeExplorationCandidatesPerMeal = 32;
constexpr int kExerciseCandidateLimit = 32;
constexpr double kReferenceExerciseMinutes = 30.0;
constexpr int kRecentPlanCount = 3;
constexpr double kRecentPlanPenalties[kRecentPlanCount] = {
    1.0, 0.6, 0.3};

void appendUniqueRecipes(QVector<Recipe>* destination,
                         QSet<QString>* acceptedIds,
                         const QVector<Recipe>& source)
{
    if (!destination || !acceptedIds) return;
    for (const Recipe& recipe : source) {
        const QString id = recipe.id.trimmed();
        if (id.isEmpty() || acceptedIds->contains(id)) continue;
        acceptedIds->insert(id);
        destination->append(recipe);
    }
}

ServiceResult<QVector<Recipe>> findRecipeCandidates(
    IRecipeRepository& repository,
    const UserProfile& user,
    double dailyTargetCalories,
    const MealRecommendationOptions& options,
    const std::optional<quint32>& weeklyRandomSeed)
{
    QVector<Recipe> candidates;
    QSet<QString> acceptedIds;

    const auto queryMeal = [&](MealType mealType, double ratio)
        -> ServiceResult<bool> {
        if (ratio <= 0.0) return ServiceResult<bool>::success(true);
        RecipeFilter filter;
        filter.mealType = mealType;
        filter.excludedIds = user.dislikedRecipeIds;
        filter.targetCalories = dailyTargetCalories * ratio;
        filter.limit = kRecipeQueryLimitPerMeal;
        const auto result = repository.findAll(filter);
        if (!result.ok) {
            return ServiceResult<bool>::failure(
                result.code, result.message, result.warnings);
        }
        const int qualityCount = std::min(
            kRecipeQualityCandidatesPerMeal,
            static_cast<int>(result.data.size()));
        appendUniqueRecipes(
            &candidates,
            &acceptedIds,
            result.data.mid(0, qualityCount));

        QVector<int> explorationIndexes;
        explorationIndexes.reserve(result.data.size() - qualityCount);
        for (int index = qualityCount;
             index < result.data.size();
             ++index) {
            explorationIndexes.append(index);
        }
        if (weeklyRandomSeed.has_value()) {
            QRandomGenerator generator(
                *weeklyRandomSeed
                ^ (0x9e3779b9U
                   * (static_cast<quint32>(mealType) + 1U)));
            for (int index = explorationIndexes.size() - 1;
                 index > 0;
                 --index) {
                const int swapIndex = generator.bounded(index + 1);
                explorationIndexes.swapItemsAt(index, swapIndex);
            }
        }

        const int explorationCount = std::min(
            kRecipeExplorationCandidatesPerMeal,
            static_cast<int>(explorationIndexes.size()));
        QVector<Recipe> explorationRecipes;
        explorationRecipes.reserve(explorationCount);
        for (int index = 0; index < explorationCount; ++index) {
            explorationRecipes.append(
                result.data.at(explorationIndexes.at(index)));
        }
        appendUniqueRecipes(
            &candidates,
            &acceptedIds,
            explorationRecipes);
        return ServiceResult<bool>::success(true);
    };

    const auto breakfast = queryMeal(
        MealType::Breakfast, options.breakfastRatio);
    if (!breakfast.ok) {
        return ServiceResult<QVector<Recipe>>::failure(
            breakfast.code, breakfast.message, breakfast.warnings);
    }
    const auto lunch = queryMeal(MealType::Lunch, options.lunchRatio);
    if (!lunch.ok) {
        return ServiceResult<QVector<Recipe>>::failure(
            lunch.code, lunch.message, lunch.warnings);
    }
    const auto dinner = queryMeal(MealType::Dinner, options.dinnerRatio);
    if (!dinner.ok) {
        return ServiceResult<QVector<Recipe>>::failure(
            dinner.code, dinner.message, dinner.warnings);
    }
    if (options.includeSnack && options.snackRatio > 0.0) {
        const auto snack = queryMeal(MealType::Snack, options.snackRatio);
        if (!snack.ok) {
            return ServiceResult<QVector<Recipe>>::failure(
                snack.code, snack.message, snack.warnings);
        }
    }

    return ServiceResult<QVector<Recipe>>::success(
        candidates,
        QStringLiteral("已从数据库按餐别和目标热量检索食谱候选。"));
}

ServiceResult<QVector<Exercise>> findExerciseCandidates(
    IExerciseRepository& repository,
    const UserProfile& user,
    double targetCalories)
{
    ExerciseFilter filter;
    filter.excludedIds = user.dislikedExerciseIds;
    filter.minimumMet = 1.5;
    filter.maximumMet = 18.0;
    filter.limit = kExerciseCandidateLimit;
    if (user.weightKg > 0.0 && targetCalories > 0.0) {
        filter.targetMet = targetCalories * 200.0
            / (3.5 * user.weightKg * kReferenceExerciseMinutes);
    }
    return repository.findAll(filter);
}

void appendRecipeIds(QSet<QString>* ids, const WeeklyPlan& plan)
{
    if (!ids) return;
    const auto appendItems = [ids](const QVector<MealPlanItem>& items) {
        for (const MealPlanItem& item : items) {
            const QString id = item.recipeId.trimmed();
            if (!id.isEmpty()) ids->insert(id);
        }
    };
    for (const DailyPlan& day : plan.days) {
        appendItems(day.meals.breakfast);
        appendItems(day.meals.lunch);
        appendItems(day.meals.dinner);
        appendItems(day.meals.snacks);
    }
}

QHash<QString, double> buildRecentRecipePenalties(
    const QVector<WeeklyPlan>& plans,
    const QDate& requestedStartDate)
{
    QHash<QString, double> penalties;
    int acceptedPlanCount = 0;
    for (const WeeklyPlan& plan : plans) {
        if (!plan.startDate.isValid()
            || plan.startDate > requestedStartDate) {
            continue;
        }

        QSet<QString> recipeIds;
        appendRecipeIds(&recipeIds, plan);
        const double penalty =
            kRecentPlanPenalties[acceptedPlanCount];
        for (const QString& id : recipeIds) {
            penalties[id] += penalty;
        }

        ++acceptedPlanCount;
        if (acceptedPlanCount >= kRecentPlanCount) break;
    }
    return penalties;
}

} // namespace

PlanGenerationService::PlanGenerationService(
    IUserRepository& userRepository,
    IExerciseRepository& exerciseRepository,
    IRecipeRepository& recipeRepository,
    IPlanRepository& planRepository,
    IHealthCalculator& healthCalculator,
    IWeeklyPlanner& weeklyPlanner,
    IFeedbackService& feedbackService)
    : userRepository_(userRepository),
      exerciseRepository_(exerciseRepository),
      recipeRepository_(recipeRepository),
      planRepository_(planRepository),
      healthCalculator_(healthCalculator),
      weeklyPlanner_(weeklyPlanner),
      feedbackService_(feedbackService)
{
}

ServiceResult<WeeklyPlan> PlanGenerationService::generateAndSave(
    const QString& userId,
    const QDate& startDate,
    const WeeklyPlanOptions& options)
{
    const QString normalizedUserId = userId.trimmed();
    if (normalizedUserId.isEmpty() || !startDate.isValid()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("INVALID_GENERATION_REQUEST"),
            QStringLiteral("用户编号和计划开始日期不能为空。"));
    }

    const auto userResult = userRepository_.findById(normalizedUserId);
    if (!userResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            userResult.code, userResult.message, userResult.warnings);
    }
    if (!userResult.data.has_value()) {
        return ServiceResult<WeeklyPlan>::failure(
            QStringLiteral("USER_NOT_FOUND"),
            QStringLiteral("当前登录用户不存在。"));
    }

    const auto calorieResult = healthCalculator_.calculate(*userResult.data);
    if (!calorieResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            calorieResult.code,
            calorieResult.message,
            calorieResult.warnings);
    }

    const auto exerciseResult = findExerciseCandidates(
        exerciseRepository_,
        *userResult.data,
        calorieResult.data.exerciseTarget);
    if (!exerciseResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            exerciseResult.code,
            exerciseResult.message,
            exerciseResult.warnings);
    }

    const auto recipeResult = findRecipeCandidates(
        recipeRepository_,
        *userResult.data,
        calorieResult.data.recommendedIntake,
        options.mealOptions,
        options.randomSeed);
    if (!recipeResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            recipeResult.code,
            recipeResult.message,
            recipeResult.warnings);
    }

    // 生成新计划前，把历史享受度反馈汇总成偏好注入推荐选项；
    // 汇总失败时保持空偏好，不阻断计划生成。
    WeeklyPlanOptions effectiveOptions = options;
    const auto recipePreference = feedbackService_.buildPreference(
        normalizedUserId, RecommendationItemType::Recipe);
    if (recipePreference.ok) {
        effectiveOptions.mealOptions.preference = recipePreference.data;
    }
    const auto exercisePreference = feedbackService_.buildPreference(
        normalizedUserId, RecommendationItemType::Exercise);
    if (exercisePreference.ok) {
        effectiveOptions.exerciseOptions.preference = exercisePreference.data;
    }

    QStringList preparationWarnings;
    const auto historyResult = planRepository_.findByUserId(
        normalizedUserId);
    if (historyResult.ok) {
        const QHash<QString, double> recentPenalties =
            buildRecentRecipePenalties(
                historyResult.data,
                startDate);
        for (auto it = recentPenalties.cbegin();
             it != recentPenalties.cend();
             ++it) {
            effectiveOptions.mealOptions.recentRecipePenalties[it.key()]
                += it.value();
        }
    } else {
        preparationWarnings.append(QStringLiteral(
            "读取历史周计划失败，本次未应用跨周食谱降重。"));
    }

    auto planResult = weeklyPlanner_.generate(
        *userResult.data,
        calorieResult.data,
        startDate,
        exerciseResult.data,
        recipeResult.data,
        effectiveOptions);
    planResult.warnings.append(calorieResult.warnings);
    planResult.warnings.append(preparationWarnings);
    if (!planResult.ok) {
        return planResult;
    }

    const auto saveResult = planRepository_.save(planResult.data);
    if (!saveResult.ok) {
        return ServiceResult<WeeklyPlan>::failure(
            saveResult.code,
            saveResult.message,
            saveResult.warnings);
    }

    QStringList warnings = planResult.warnings;
    warnings.append(saveResult.warnings);
    return ServiceResult<WeeklyPlan>::success(
        saveResult.data,
        QStringLiteral("周计划已生成并保存。"),
        std::move(warnings));
}

