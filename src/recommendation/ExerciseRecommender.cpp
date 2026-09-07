#include "recommendation/ExerciseRecommender.h"

#include <QMap>
#include <QRandomGenerator>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace {

constexpr double kMaximumAllowedToleranceRatio = 0.10;
constexpr int kMaximumSupportedExerciseItems = 3;
constexpr double kComparisonEpsilon = 1e-9;
constexpr double kCalorieBucketSize = 5.0;
constexpr int kMaximumStatesPerCalorieBucket = 6;
constexpr int kExerciseBeamWidth = 800;
constexpr int kMaximumRankedPlans = 32;
constexpr int kRandomPlanPoolSize = 5;
constexpr double kRecentExposurePenaltyWeight = 0.75;
constexpr double kEquivalentGoalPenalty = 0.25;
constexpr double kEquivalentCalorieRatio = 0.03;

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

bool isValidRecentExercisePenalties(
    const QHash<QString, double>& penalties)
{
    for (auto it = penalties.cbegin(); it != penalties.cend(); ++it) {
        if (it.key().trimmed().isEmpty()
            || !isFiniteNonNegative(it.value())) {
            return false;
        }
    }
    return true;
}

QVector<Exercise> filterEligibleExercises(
    const UserProfile& user,
    const QVector<Exercise>& exerciseDatabase,
    const ExerciseRecommendationOptions& options)
{
    QSet<QString> excludedIds;
    for (const QString& id : user.dislikedExerciseIds) {
        excludedIds.insert(id.trimmed());
    }
    for (const QString& id : options.excludedExerciseIds) {
        excludedIds.insert(id.trimmed());
    }

    QVector<Exercise> eligibleExercises;
    eligibleExercises.reserve(exerciseDatabase.size());
    QSet<QString> acceptedIds;
    for (const Exercise& exercise : exerciseDatabase) {
        const QString normalizedId = exercise.id.trimmed();
        if (normalizedId.isEmpty()
            || !isFinitePositive(exercise.metValue)
            || excludedIds.contains(normalizedId)
            || acceptedIds.contains(normalizedId)) {
            continue;
        }

        Exercise normalizedExercise = exercise;
        normalizedExercise.id = normalizedId;
        eligibleExercises.append(std::move(normalizedExercise));
        acceptedIds.insert(normalizedId);
    }
    return eligibleExercises;
}

double calculateCaloriesBurned(
    double metValue,
    double weightKg,
    int durationMinutes)
{
    return metValue * 3.5 * weightKg / 200.0 * durationMinutes;
}

double exerciseGoalPenalty(
    const Exercise& exercise,
    ExerciseGoal goal,
    const ExerciseRecommendationOptions& options)
{
    double preferredMinimumMet = 2.0;
    double preferredMaximumMet = 3.5;
    double categoryPenalty = 0.0;

    switch (goal) {
    case ExerciseGoal::LightHealth:
        preferredMinimumMet = 2.0;
        preferredMaximumMet = 3.5;
        break;
    case ExerciseGoal::BuildFitness:
        preferredMinimumMet = 4.0;
        preferredMaximumMet = 6.0;
        break;
    case ExerciseGoal::MuscleGain:
        preferredMinimumMet = 6.0;
        preferredMaximumMet = 7.5;
        if (exercise.category != ExerciseCategory::Strength) {
            categoryPenalty = 100.0;
        }
        break;
    }

    double intensityPenalty = 0.0;
    if (exercise.metValue < preferredMinimumMet) {
        intensityPenalty = preferredMinimumMet - exercise.metValue;
    } else if (exercise.metValue > preferredMaximumMet) {
        intensityPenalty = exercise.metValue - preferredMaximumMet;
    }

    const double feedbackAdjustment =
        options.preference.itemWeights.value(exercise.id, 1.0) - 1.0;
    const double recentExposurePenalty =
        options.recentExercisePenalties.value(exercise.id, 0.0)
        * kRecentExposurePenaltyWeight;
    return categoryPenalty
        + intensityPenalty
        + recentExposurePenalty
        - feedbackAdjustment;
}

int totalDurationOf(const QVector<ExercisePlanItem>& items)
{
    int total = 0;
    for (const ExercisePlanItem& item : items) {
        total += item.durationMinutes;
    }
    return total;
}

struct PartialExercisePlan {
    QVector<ExercisePlanItem> items;
    int lastExerciseIndex = -1;
    double totalCalories = 0.0;
    double goalPenaltyTotal = 0.0;
};

double averageGoalPenalty(const PartialExercisePlan& plan)
{
    return plan.items.isEmpty()
        ? 0.0
        : plan.goalPenaltyTotal / plan.items.size();
}

bool isBetterPartial(
    const PartialExercisePlan& candidate,
    const PartialExercisePlan& current,
    double targetCalories,
    int maximumItems)
{
    const double candidateGoal = averageGoalPenalty(candidate);
    const double currentGoal = averageGoalPenalty(current);
    if (std::abs(candidateGoal - currentGoal) > kComparisonEpsilon) {
        return candidateGoal < currentGoal;
    }

    const double progressTarget = targetCalories
        * static_cast<double>(candidate.items.size()) / maximumItems;
    const double candidateDifference = std::abs(
        candidate.totalCalories - progressTarget);
    const double currentDifference = std::abs(
        current.totalCalories - progressTarget);
    if (std::abs(candidateDifference - currentDifference)
        > kComparisonEpsilon) {
        return candidateDifference < currentDifference;
    }
    return totalDurationOf(candidate.items)
        < totalDurationOf(current.items);
}

bool isBetterFinalPlan(
    const PartialExercisePlan& candidate,
    const PartialExercisePlan& current)
{
    const double candidateGoal = averageGoalPenalty(candidate);
    const double currentGoal = averageGoalPenalty(current);
    if (std::abs(candidateGoal - currentGoal) > kComparisonEpsilon) {
        return candidateGoal < currentGoal;
    }
    if (std::abs(candidate.totalCalories - current.totalCalories)
        > kComparisonEpsilon) {
        return candidate.totalCalories < current.totalCalories;
    }
    const int candidateDuration = totalDurationOf(candidate.items);
    const int currentDuration = totalDurationOf(current.items);
    if (candidateDuration != currentDuration) {
        return candidateDuration < currentDuration;
    }
    return candidate.items.size() < current.items.size();
}

void appendBoundedState(
    QMap<int, QVector<PartialExercisePlan>>& buckets,
    PartialExercisePlan state,
    double targetCalories,
    int maximumItems)
{
    const int bucket = static_cast<int>(std::floor(
        state.totalCalories / kCalorieBucketSize));
    QVector<PartialExercisePlan>& states = buckets[bucket];
    states.append(std::move(state));
    std::stable_sort(
        states.begin(),
        states.end(),
        [targetCalories, maximumItems](
            const PartialExercisePlan& left,
            const PartialExercisePlan& right) {
            return isBetterPartial(
                left, right, targetCalories, maximumItems);
        });
    if (states.size() > kMaximumStatesPerCalorieBucket) {
        states.resize(kMaximumStatesPerCalorieBucket);
    }
}

void considerFinalPlan(
    QVector<PartialExercisePlan>& rankedPlans,
    const PartialExercisePlan& candidate)
{
    rankedPlans.append(candidate);
    std::stable_sort(
        rankedPlans.begin(),
        rankedPlans.end(),
        isBetterFinalPlan);
    if (rankedPlans.size() > kMaximumRankedPlans) {
        rankedPlans.resize(kMaximumRankedPlans);
    }
}

std::optional<QVector<ExercisePlanItem>> findBestBoundedPlan(
    double weightKg,
    double targetCalories,
    const QVector<Exercise>& eligibleExercises,
    const ExerciseRecommendationOptions& options,
    ExerciseGoal goal)
{
    const double maximumCalories =
        targetCalories * (1.0 + options.upperToleranceRatio);
    QVector<PartialExercisePlan> beam(1);
    QVector<PartialExercisePlan> rankedPlans;

    for (int itemCount = 1;
         itemCount <= options.maximumExerciseItems;
         ++itemCount) {
        QMap<int, QVector<PartialExercisePlan>> nextBuckets;
        for (const PartialExercisePlan& partial : beam) {
            for (int exerciseIndex = partial.lastExerciseIndex + 1;
                 exerciseIndex < eligibleExercises.size();
                 ++exerciseIndex) {
                const Exercise& exercise =
                    eligibleExercises.at(exerciseIndex);
                int duration = options.minimumDurationMinutes;
                while (duration
                       <= options.maximumDurationMinutesPerExercise) {
                    const double calories = calculateCaloriesBurned(
                        exercise.metValue,
                        weightKg,
                        duration);
                    const double nextCalories =
                        partial.totalCalories + calories;
                    if (!std::isfinite(calories)
                        || !std::isfinite(nextCalories)
                        || nextCalories
                            > maximumCalories + kComparisonEpsilon) {
                        break;
                    }

                    PartialExercisePlan candidate = partial;
                    candidate.items.append({
                        exercise.id,
                        exercise.name,
                        duration,
                        calories});
                    candidate.lastExerciseIndex = exerciseIndex;
                    candidate.totalCalories = nextCalories;
                    candidate.goalPenaltyTotal += exerciseGoalPenalty(
                        exercise,
                        goal,
                        options);

                    appendBoundedState(
                        nextBuckets,
                        candidate,
                        targetCalories,
                        options.maximumExerciseItems);
                    if (nextCalories + kComparisonEpsilon
                        >= targetCalories) {
                        considerFinalPlan(rankedPlans, candidate);
                    }

                    if (options.maximumDurationMinutesPerExercise - duration
                        < options.durationStepMinutes) {
                        break;
                    }
                    duration += options.durationStepMinutes;
                }
            }
        }

        beam.clear();
        for (auto it = nextBuckets.begin();
             it != nextBuckets.end();
             ++it) {
            beam += std::move(it.value());
        }
        std::stable_sort(
            beam.begin(),
            beam.end(),
            [targetCalories, &options](
                const PartialExercisePlan& left,
                const PartialExercisePlan& right) {
                return isBetterPartial(
                    left,
                    right,
                    targetCalories,
                    options.maximumExerciseItems);
            });
        if (beam.size() > kExerciseBeamWidth) {
            beam.resize(kExerciseBeamWidth);
        }
        if (beam.isEmpty()) break;
    }

    if (rankedPlans.isEmpty()) {
        return std::nullopt;
    }

    if (options.randomSeed.has_value()) {
        const PartialExercisePlan& best = rankedPlans.first();
        const double maximumEquivalentCalories =
            best.totalCalories
            + std::max(kCalorieBucketSize,
                       targetCalories * kEquivalentCalorieRatio);
        int poolSize = 1;
        while (poolSize < rankedPlans.size()
               && poolSize < kRandomPlanPoolSize
               && averageGoalPenalty(rankedPlans.at(poolSize))
                    <= averageGoalPenalty(best)
                        + kEquivalentGoalPenalty
                        + kComparisonEpsilon
               && rankedPlans.at(poolSize).totalCalories
                    <= maximumEquivalentCalories + kComparisonEpsilon) {
            ++poolSize;
        }
        QRandomGenerator generator(*options.randomSeed ^ 0xc2b2ae35U);
        return rankedPlans.at(generator.bounded(poolSize)).items;
    }
    return rankedPlans.first().items;
}

} // namespace

ServiceResult<QVector<ExercisePlanItem>> ExerciseRecommender::generate(
    const UserProfile& user,
    double targetCalories,
    const QVector<Exercise>& exerciseDatabase,
    const ExerciseRecommendationOptions& options) const
{
    if (!isFinitePositive(targetCalories)) {
        return ServiceResult<QVector<ExercisePlanItem>>::failure(
            QStringLiteral("INVALID_TARGET"),
            QStringLiteral("目标运动热量必须是大于 0 的有限数值。"));
    }
    if (!isFinitePositive(user.weightKg)) {
        return ServiceResult<QVector<ExercisePlanItem>>::failure(
            QStringLiteral("INVALID_USER"),
            QStringLiteral("用户体重必须是大于 0 的有限数值。"));
    }
    if (exerciseDatabase.isEmpty()) {
        return ServiceResult<QVector<ExercisePlanItem>>::failure(
            QStringLiteral("EMPTY_EXERCISE_DATABASE"),
            QStringLiteral("运动数据库为空，无法生成运动处方。"));
    }

    const bool invalidDurationOptions =
        options.durationStepMinutes <= 0
        || options.minimumDurationMinutes <= 0
        || options.maximumDurationMinutesPerExercise
            < options.minimumDurationMinutes;
    const bool invalidItemCount =
        options.maximumExerciseItems <= 0
        || options.maximumExerciseItems > kMaximumSupportedExerciseItems;
    const bool invalidTolerance =
        !std::isfinite(options.upperToleranceRatio)
        || options.upperToleranceRatio < 0.0
        || options.upperToleranceRatio
            > kMaximumAllowedToleranceRatio + kComparisonEpsilon;

    if (invalidDurationOptions
        || invalidItemCount
        || invalidTolerance
        || !isValidPreference(options.preference)
        || !isValidRecentExercisePenalties(
            options.recentExercisePenalties)) {
        return ServiceResult<QVector<ExercisePlanItem>>::failure(
            QStringLiteral("INVALID_OPTIONS"),
            QStringLiteral(
                "运动推荐选项不合法：项目数必须为 1～3，时长和步长必须"
                "为正数，允许超出比例必须为 0～10%，近期运动惩罚必须为"
                "非负有限数。"));
    }

    const QVector<Exercise> eligibleExercises = filterEligibleExercises(
        user,
        exerciseDatabase,
        options);
    if (eligibleExercises.isEmpty()) {
        return ServiceResult<QVector<ExercisePlanItem>>::failure(
            QStringLiteral("NO_ELIGIBLE_EXERCISE"),
            QStringLiteral(
                "过滤无效、重复、不喜欢及显式排除的运动后，没有可推荐项目。"));
    }

    const auto bestPlan = findBestBoundedPlan(
        user.weightKg,
        targetCalories,
        eligibleExercises,
        options,
        user.exerciseGoal);
    if (bestPlan.has_value()) {
        return ServiceResult<QVector<ExercisePlanItem>>::success(
            *bestPlan,
            QStringLiteral(
                "已通过有界搜索生成满足热量约束并匹配运动目标的方案。"));
    }

    return ServiceResult<QVector<ExercisePlanItem>>::failure(
        QStringLiteral("NO_FEASIBLE_EXERCISE_PLAN"),
        QStringLiteral(
            "在当前项目数、时长和热量容差限制内找不到可行运动方案。"));
}
