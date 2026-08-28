#include "recommendation/ExerciseRecommender.h"

#include <QSet>

#include <cmath>
#include <optional>
#include <utility>

namespace {

// 老师要求运动总消耗不得超过目标值的 10%。
constexpr double kMaximumAllowedToleranceRatio = 0.10;

// 基础版本最多组合三项不同运动。
constexpr int kMaximumSupportedExerciseItems = 3;

// 避免浮点计算产生极小误差后误判 0.10 为非法值。
constexpr double kComparisonEpsilon = 1e-9;

bool isFinitePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

QVector<Exercise> filterEligibleExercises(
    const UserProfile& user,
    const QVector<Exercise>& exerciseDatabase,
    const ExerciseRecommendationOptions& options)
{
    // QSet 用于快速判断某个运动 ID 是否应被排除，同时自动合并重复项。
    QSet<QString> excludedIds;
    for (const QString& id : user.dislikedExerciseIds) {
        excludedIds.insert(id.trimmed());
    }
    for (const QString& id : options.excludedExerciseIds) {
        excludedIds.insert(id.trimmed());
    }

    QVector<Exercise> eligibleExercises;
    eligibleExercises.reserve(exerciseDatabase.size());

    // 同一个 ID 只保留数据库中第一次出现的记录，避免最终计划重复选择
    // 实际上属于同一种运动的数据。
    QSet<QString> acceptedIds;

    for (const Exercise& exercise : exerciseDatabase) {
        const QString normalizedId = exercise.id.trimmed();

        // ID 是计划项与数据库之间的关联键，空 ID 无法安全写入计划。
        const bool hasValidId = !normalizedId.isEmpty();
        const bool hasValidMet = isFinitePositive(exercise.metValue);
        const bool isExcluded = excludedIds.contains(normalizedId);
        const bool isDuplicate = acceptedIds.contains(normalizedId);

        if (!hasValidId || !hasValidMet || isExcluded || isDuplicate) {
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
    // 通用 MET 估算公式：
    // 千卡 = MET × 3.5 × 体重(kg) ÷ 200 × 时长(min)。
    return metValue * 3.5 * weightKg / 200.0 * durationMinutes;
}

std::optional<ExercisePlanItem> findBestSingleExercise(
    double weightKg,
    double targetCalories,
    const QVector<Exercise>& eligibleExercises,
    const ExerciseRecommendationOptions& options)
{
    const double maximumCalories =
        targetCalories * (1.0 + options.upperToleranceRatio);
    std::optional<ExercisePlanItem> bestItem;

    for (const Exercise& exercise : eligibleExercises) {
        int duration = options.minimumDurationMinutes;

        while (duration <= options.maximumDurationMinutesPerExercise) {
            const double calories = calculateCaloriesBurned(
                exercise.metValue,
                weightKg,
                duration);

            const bool reachesTarget =
                calories + kComparisonEpsilon >= targetCalories;
            const bool staysWithinUpperBound =
                calories <= maximumCalories + kComparisonEpsilon;

            if (std::isfinite(calories)
                && reachesTarget
                && staysWithinUpperBound) {
                ExercisePlanItem candidate;
                candidate.exerciseId = exercise.id;
                candidate.exerciseName = exercise.name;
                candidate.durationMinutes = duration;
                candidate.caloriesBurned = calories;

                // 首先选择最接近目标值的方案；热量相同时选择时长更短的方案。
                const bool isBetter =
                    !bestItem.has_value()
                    || candidate.caloriesBurned
                        < bestItem->caloriesBurned - kComparisonEpsilon
                    || (std::abs(candidate.caloriesBurned
                                 - bestItem->caloriesBurned)
                            <= kComparisonEpsilon
                        && candidate.durationMinutes
                            < bestItem->durationMinutes);

                if (isBetter) {
                    bestItem = std::move(candidate);
                }

                // 对同一运动而言，时长继续增加只会离目标更远。
                break;
            }

            // 当前热量已经超过上限，后续更长时长不可能成为合法方案。
            if (!std::isfinite(calories)
                || calories > maximumCalories + kComparisonEpsilon) {
                break;
            }

            // 先检查剩余空间，避免 duration + step 发生整数溢出。
            if (options.maximumDurationMinutesPerExercise - duration
                < options.durationStepMinutes) {
                break;
            }
            duration += options.durationStepMinutes;
        }
    }

    return bestItem;
}

double totalCaloriesOf(const QVector<ExercisePlanItem>& items)
{
    double total = 0.0;
    for (const ExercisePlanItem& item : items) {
        total += item.caloriesBurned;
    }
    return total;
}

int totalDurationOf(const QVector<ExercisePlanItem>& items)
{
    int total = 0;
    for (const ExercisePlanItem& item : items) {
        total += item.durationMinutes;
    }
    return total;
}

std::optional<QVector<ExercisePlanItem>> findBestTwoExercises(
    double weightKg,
    double targetCalories,
    const QVector<Exercise>& eligibleExercises,
    const ExerciseRecommendationOptions& options)
{
    if (options.maximumExerciseItems < 2 || eligibleExercises.size() < 2) {
        return std::nullopt;
    }

    const double maximumCalories =
        targetCalories * (1.0 + options.upperToleranceRatio);
    std::optional<QVector<ExercisePlanItem>> bestPlan;

    // i < j 保证一份方案不会重复使用同一种运动，也不会把
    // “跑步 + 步行”和“步行 + 跑步”当作两个不同组合。
    for (qsizetype i = 0; i < eligibleExercises.size() - 1; ++i) {
        const Exercise& firstExercise = eligibleExercises.at(i);

        for (qsizetype j = i + 1; j < eligibleExercises.size(); ++j) {
            const Exercise& secondExercise = eligibleExercises.at(j);
            int firstDuration = options.minimumDurationMinutes;

            while (firstDuration
                   <= options.maximumDurationMinutesPerExercise) {
                const double firstCalories = calculateCaloriesBurned(
                    firstExercise.metValue,
                    weightKg,
                    firstDuration);

                if (!std::isfinite(firstCalories)
                    || firstCalories > maximumCalories + kComparisonEpsilon) {
                    break;
                }

                int secondDuration = options.minimumDurationMinutes;
                while (secondDuration
                       <= options.maximumDurationMinutesPerExercise) {
                    const double secondCalories = calculateCaloriesBurned(
                        secondExercise.metValue,
                        weightKg,
                        secondDuration);
                    const double totalCalories =
                        firstCalories + secondCalories;

                    if (!std::isfinite(secondCalories)
                        || !std::isfinite(totalCalories)
                        || totalCalories
                            > maximumCalories + kComparisonEpsilon) {
                        break;
                    }

                    if (totalCalories + kComparisonEpsilon
                        >= targetCalories) {
                        ExercisePlanItem firstItem;
                        firstItem.exerciseId = firstExercise.id;
                        firstItem.exerciseName = firstExercise.name;
                        firstItem.durationMinutes = firstDuration;
                        firstItem.caloriesBurned = firstCalories;

                        ExercisePlanItem secondItem;
                        secondItem.exerciseId = secondExercise.id;
                        secondItem.exerciseName = secondExercise.name;
                        secondItem.durationMinutes = secondDuration;
                        secondItem.caloriesBurned = secondCalories;

                        QVector<ExercisePlanItem> candidate{
                            firstItem,
                            secondItem};

                        const double bestCalories = bestPlan.has_value()
                            ? totalCaloriesOf(*bestPlan)
                            : 0.0;
                        const bool isBetter =
                            !bestPlan.has_value()
                            || totalCalories
                                < bestCalories - kComparisonEpsilon
                            || (std::abs(totalCalories - bestCalories)
                                    <= kComparisonEpsilon
                                && totalDurationOf(candidate)
                                    < totalDurationOf(*bestPlan));

                        if (isBetter) {
                            bestPlan = std::move(candidate);
                        }

                        // 固定第一项时长后，继续增加第二项时长只会
                        // 使总热量离目标更远。
                        break;
                    }

                    if (options.maximumDurationMinutesPerExercise
                            - secondDuration
                        < options.durationStepMinutes) {
                        break;
                    }
                    secondDuration += options.durationStepMinutes;
                }

                if (options.maximumDurationMinutesPerExercise
                        - firstDuration
                    < options.durationStepMinutes) {
                    break;
                }
                firstDuration += options.durationStepMinutes;
            }
        }
    }

    return bestPlan;
}

std::optional<QVector<ExercisePlanItem>> findBestThreeExercises(
    double weightKg,
    double targetCalories,
    const QVector<Exercise>& eligibleExercises,
    const ExerciseRecommendationOptions& options)
{
    if (options.maximumExerciseItems < 3 || eligibleExercises.size() < 3) {
        return std::nullopt;
    }

    const double maximumCalories =
        targetCalories * (1.0 + options.upperToleranceRatio);
    std::optional<QVector<ExercisePlanItem>> bestPlan;

    // i < j < k 保证三项运动彼此不同，并消除排列顺序造成的重复搜索。
    for (qsizetype i = 0; i < eligibleExercises.size() - 2; ++i) {
        const Exercise& firstExercise = eligibleExercises.at(i);
        for (qsizetype j = i + 1; j < eligibleExercises.size() - 1; ++j) {
            const Exercise& secondExercise = eligibleExercises.at(j);
            for (qsizetype k = j + 1; k < eligibleExercises.size(); ++k) {
                const Exercise& thirdExercise = eligibleExercises.at(k);
                int firstDuration = options.minimumDurationMinutes;

                while (firstDuration
                       <= options.maximumDurationMinutesPerExercise) {
                    const double firstCalories = calculateCaloriesBurned(
                        firstExercise.metValue,
                        weightKg,
                        firstDuration);
                    if (!std::isfinite(firstCalories)
                        || firstCalories
                            > maximumCalories + kComparisonEpsilon) {
                        break;
                    }

                    int secondDuration = options.minimumDurationMinutes;
                    while (secondDuration
                           <= options.maximumDurationMinutesPerExercise) {
                        const double secondCalories = calculateCaloriesBurned(
                            secondExercise.metValue,
                            weightKg,
                            secondDuration);
                        const double firstTwoCalories =
                            firstCalories + secondCalories;
                        if (!std::isfinite(firstTwoCalories)
                            || firstTwoCalories
                                > maximumCalories + kComparisonEpsilon) {
                            break;
                        }

                        int thirdDuration = options.minimumDurationMinutes;
                        while (thirdDuration
                               <= options.maximumDurationMinutesPerExercise) {
                            const double thirdCalories = calculateCaloriesBurned(
                                thirdExercise.metValue,
                                weightKg,
                                thirdDuration);
                            const double totalCalories =
                                firstTwoCalories + thirdCalories;

                            if (!std::isfinite(thirdCalories)
                                || !std::isfinite(totalCalories)
                                || totalCalories
                                    > maximumCalories + kComparisonEpsilon) {
                                break;
                            }

                            if (totalCalories + kComparisonEpsilon
                                >= targetCalories) {
                                ExercisePlanItem firstItem{
                                    firstExercise.id,
                                    firstExercise.name,
                                    firstDuration,
                                    firstCalories};
                                ExercisePlanItem secondItem{
                                    secondExercise.id,
                                    secondExercise.name,
                                    secondDuration,
                                    secondCalories};
                                ExercisePlanItem thirdItem{
                                    thirdExercise.id,
                                    thirdExercise.name,
                                    thirdDuration,
                                    thirdCalories};

                                QVector<ExercisePlanItem> candidate{
                                    firstItem,
                                    secondItem,
                                    thirdItem};
                                const double bestCalories = bestPlan.has_value()
                                    ? totalCaloriesOf(*bestPlan)
                                    : 0.0;
                                const bool isBetter =
                                    !bestPlan.has_value()
                                    || totalCalories
                                        < bestCalories - kComparisonEpsilon
                                    || (std::abs(totalCalories - bestCalories)
                                            <= kComparisonEpsilon
                                        && totalDurationOf(candidate)
                                            < totalDurationOf(*bestPlan));

                                if (isBetter) {
                                    bestPlan = std::move(candidate);
                                }

                                // 固定前两项时长后，更长的第三项只会增加偏差。
                                break;
                            }

                            if (options.maximumDurationMinutesPerExercise
                                    - thirdDuration
                                < options.durationStepMinutes) {
                                break;
                            }
                            thirdDuration += options.durationStepMinutes;
                        }

                        if (options.maximumDurationMinutesPerExercise
                                - secondDuration
                            < options.durationStepMinutes) {
                            break;
                        }
                        secondDuration += options.durationStepMinutes;
                    }

                    if (options.maximumDurationMinutesPerExercise
                            - firstDuration
                        < options.durationStepMinutes) {
                        break;
                    }
                    firstDuration += options.durationStepMinutes;
                }
            }
        }
    }

    return bestPlan;
}

} // namespace

ServiceResult<QVector<ExercisePlanItem>> ExerciseRecommender::generate(
    const UserProfile& user,
    double targetCalories,
    const QVector<Exercise>& exerciseDatabase,
    const ExerciseRecommendationOptions& options) const
{
    // 目标热量必须是正常、有限的正数。
    if (!isFinitePositive(targetCalories)) {
        return ServiceResult<QVector<ExercisePlanItem>>::failure(
            QStringLiteral("INVALID_TARGET"),
            QStringLiteral("目标运动热量必须是大于 0 的有限数值。"));
    }

    // MET 热量公式需要使用用户体重，其他用户字段不属于本模块的职责。
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

    if (invalidDurationOptions || invalidItemCount || invalidTolerance) {
        return ServiceResult<QVector<ExercisePlanItem>>::failure(
            QStringLiteral("INVALID_OPTIONS"),
            QStringLiteral(
                "运动推荐选项不合法：项目数必须为 1～3，"
                "时长和步长必须为正数，允许超出比例必须为 0～10%。"));
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

    const std::optional<ExercisePlanItem> bestSingleExercise =
        findBestSingleExercise(
            user.weightKg,
            targetCalories,
            eligibleExercises,
            options);

    if (bestSingleExercise.has_value()) {
        return ServiceResult<QVector<ExercisePlanItem>>::success(
            {*bestSingleExercise},
            QStringLiteral("已生成满足目标热量范围的单项运动方案。"));
    }

    const std::optional<QVector<ExercisePlanItem>> bestTwoExercises =
        findBestTwoExercises(
            user.weightKg,
            targetCalories,
            eligibleExercises,
            options);

    if (bestTwoExercises.has_value()) {
        return ServiceResult<QVector<ExercisePlanItem>>::success(
            *bestTwoExercises,
            QStringLiteral("已生成满足目标热量范围的两项运动组合。"));
    }

    const std::optional<QVector<ExercisePlanItem>> bestThreeExercises =
        findBestThreeExercises(
            user.weightKg,
            targetCalories,
            eligibleExercises,
            options);

    if (bestThreeExercises.has_value()) {
        return ServiceResult<QVector<ExercisePlanItem>>::success(
            *bestThreeExercises,
            QStringLiteral("已生成满足目标热量范围的三项运动组合。"));
    }

    // 所有允许的 1～3 项组合均已搜索完毕，仍不存在合法解。
    return ServiceResult<QVector<ExercisePlanItem>>::failure(
        QStringLiteral("NO_FEASIBLE_EXERCISE_PLAN"),
        QStringLiteral(
            "在当前项目数、时长和热量容差限制内找不到可行运动方案。"));
}
