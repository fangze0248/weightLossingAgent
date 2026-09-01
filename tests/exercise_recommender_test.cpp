#include "recommendation/ExerciseRecommender.h"

#include <cmath>
#include <limits>

int main()
{
    // 先保留公共运动推荐选项的默认契约检查。
    const ExerciseRecommendationOptions options;

    if (options.durationStepMinutes != 5) {
        return 1;
    }

    if (options.minimumDurationMinutes != 5) {
        return 2;
    }

    if (options.maximumDurationMinutesPerExercise != 120) {
        return 3;
    }

    if (options.maximumExerciseItems != 3) {
        return 4;
    }

    if (std::abs(options.upperToleranceRatio - 0.10) > 1e-9) {
        return 5;
    }

    ExerciseRecommender recommender;

    UserProfile validUser;
    validUser.weightKg = 70.0;

    Exercise validExercise;
    validExercise.id = QStringLiteral("running");
    validExercise.name = QStringLiteral("跑步");
    validExercise.metValue = 8.0;

    const QVector<Exercise> validDatabase{validExercise};

    const auto invalidTargetResult = recommender.generate(
        validUser,
        0.0,
        validDatabase);

    if (invalidTargetResult.code != QStringLiteral("INVALID_TARGET")) {
        return 6;
    }

    const auto infiniteTargetResult = recommender.generate(
        validUser,
        std::numeric_limits<double>::infinity(),
        validDatabase);

    if (infiniteTargetResult.code != QStringLiteral("INVALID_TARGET")) {
        return 7;
    }

    UserProfile invalidUser = validUser;
    invalidUser.weightKg = 0.0;

    const auto invalidUserResult = recommender.generate(
        invalidUser,
        300.0,
        validDatabase);

    if (invalidUserResult.code != QStringLiteral("INVALID_USER")) {
        return 8;
    }

    const auto emptyDatabaseResult = recommender.generate(
        validUser,
        300.0,
        {});

    if (emptyDatabaseResult.code
        != QStringLiteral("EMPTY_EXERCISE_DATABASE")) {
        return 9;
    }

    ExerciseRecommendationOptions invalidOptions = options;
    invalidOptions.maximumExerciseItems = 4;

    const auto invalidOptionsResult = recommender.generate(
        validUser,
        300.0,
        validDatabase,
        invalidOptions);

    if (invalidOptionsResult.code != QStringLiteral("INVALID_OPTIONS")) {
        return 10;
    }

    // 300 千卡无法由唯一的跑步项目按 5 分钟离散时长在 10% 上限内达到。
    const auto validInputResult = recommender.generate(
        validUser,
        300.0,
        validDatabase,
        options);

    if (validInputResult.code
        != QStringLiteral("NO_FEASIBLE_EXERCISE_PLAN")) {
        return 11;
    }

    Exercise invalidIdExercise = validExercise;
    invalidIdExercise.id = QStringLiteral("   ");

    Exercise invalidMetExercise = validExercise;
    invalidMetExercise.id = QStringLiteral("invalid-met");
    invalidMetExercise.metValue = 0.0;

    const auto invalidDataResult = recommender.generate(
        validUser,
        300.0,
        {invalidIdExercise, invalidMetExercise},
        options);

    if (invalidDataResult.code
        != QStringLiteral("NO_ELIGIBLE_EXERCISE")) {
        return 12;
    }

    UserProfile userWithDislike = validUser;
    userWithDislike.dislikedExerciseIds.append(QStringLiteral("running"));

    const auto dislikedResult = recommender.generate(
        userWithDislike,
        300.0,
        validDatabase,
        options);

    if (dislikedResult.code != QStringLiteral("NO_ELIGIBLE_EXERCISE")) {
        return 13;
    }

    ExerciseRecommendationOptions excludedOptions = options;
    excludedOptions.excludedExerciseIds.append(QStringLiteral("running"));

    const auto explicitlyExcludedResult = recommender.generate(
        validUser,
        300.0,
        validDatabase,
        excludedOptions);

    if (explicitlyExcludedResult.code
        != QStringLiteral("NO_ELIGIBLE_EXERCISE")) {
        return 14;
    }

    // 数据库中只要还有一个合法且未被排除的项目，就应该通过过滤阶段。
    Exercise walkingExercise;
    walkingExercise.id = QStringLiteral("walking");
    walkingExercise.name = QStringLiteral("步行");
    walkingExercise.metValue = 3.5;

    const auto mixedDatabaseResult = recommender.generate(
        userWithDislike,
        300.0,
        {validExercise, invalidMetExercise, walkingExercise},
        options);

    if (!mixedDatabaseResult.ok || mixedDatabaseResult.data.size() != 1) {
        return 15;
    }

    // 70 kg 用户进行 8 MET 跑步时，每分钟约消耗 9.8 千卡；
    // 20 分钟恰好消耗 196 千卡。
    const auto exactSingleExerciseResult = recommender.generate(
        validUser,
        196.0,
        validDatabase,
        options);

    if (!exactSingleExerciseResult.ok
        || exactSingleExerciseResult.code != QStringLiteral("OK")
        || exactSingleExerciseResult.data.size() != 1) {
        return 16;
    }

    const ExercisePlanItem exactItem = exactSingleExerciseResult.data.first();
    if (exactItem.exerciseId != QStringLiteral("running")
        || exactItem.exerciseName != QStringLiteral("跑步")
        || exactItem.durationMinutes != 20
        || std::abs(exactItem.caloriesBurned - 196.0) > 1e-9) {
        return 17;
    }

    // 最大时长不足时不能偷偷突破限制来凑够目标热量。
    ExerciseRecommendationOptions shortDurationOptions = options;
    shortDurationOptions.maximumDurationMinutesPerExercise = 15;

    const auto durationLimitedResult = recommender.generate(
        validUser,
        196.0,
        validDatabase,
        shortDurationOptions);

    if (durationLimitedResult.code
        != QStringLiteral("NO_FEASIBLE_EXERCISE_PLAN")) {
        return 18;
    }

    // 20 分钟跑步为 196 千卡，超过 175 千卡目标的 10% 上限
    // （192.5 千卡），因此也不能作为成功方案。
    const auto upperBoundResult = recommender.generate(
        validUser,
        175.0,
        validDatabase,
        options);

    if (upperBoundResult.code
        != QStringLiteral("NO_FEASIBLE_EXERCISE_PLAN")) {
        return 19;
    }

    Exercise cyclingExercise;
    cyclingExercise.id = QStringLiteral("cycling");
    cyclingExercise.name = QStringLiteral("骑行");
    cyclingExercise.metValue = 4.0;

    // 最大单项时长限制为 15 分钟时，跑步最多消耗 147 千卡，
    // 骑行最多消耗 73.5 千卡，二者都无法单独达到 190 千卡；
    // 组合后可以用 15 分钟跑步 + 10 分钟骑行达到 196 千卡。
    ExerciseRecommendationOptions twoItemOptions = options;
    twoItemOptions.maximumDurationMinutesPerExercise = 15;

    const auto twoItemResult = recommender.generate(
        validUser,
        190.0,
        {validExercise, cyclingExercise},
        twoItemOptions);

    if (!twoItemResult.ok || twoItemResult.data.size() != 2) {
        return 20;
    }

    const double twoItemCalories =
        twoItemResult.data.at(0).caloriesBurned
        + twoItemResult.data.at(1).caloriesBurned;
    if (twoItemResult.data.at(0).exerciseId
            == twoItemResult.data.at(1).exerciseId
        || twoItemCalories + 1e-9 < 190.0
        || twoItemCalories > 209.0 + 1e-9) {
        return 21;
    }

    // 调用方要求最多一项时，即使存在合法两项组合也不能返回它。
    ExerciseRecommendationOptions oneItemOnlyOptions = twoItemOptions;
    oneItemOnlyOptions.maximumExerciseItems = 1;

    const auto oneItemOnlyResult = recommender.generate(
        validUser,
        190.0,
        {validExercise, cyclingExercise},
        oneItemOnlyOptions);

    if (oneItemOnlyResult.code
        != QStringLiteral("NO_FEASIBLE_EXERCISE_PLAN")) {
        return 22;
    }

    Exercise swimmingExercise;
    swimmingExercise.id = QStringLiteral("swimming");
    swimmingExercise.name = QStringLiteral("游泳");
    swimmingExercise.metValue = 6.0;

    // 每项只允许 5 分钟时，三项分别消耗 49、36.75、24.5 千卡。
    // 任意一项或两项都达不到 105 千卡，三项合计 110.25 千卡，
    // 位于 105～115.5 千卡的合法范围内。
    ExerciseRecommendationOptions threeItemOptions = options;
    threeItemOptions.maximumDurationMinutesPerExercise = 5;

    const auto threeItemResult = recommender.generate(
        validUser,
        105.0,
        {validExercise, swimmingExercise, cyclingExercise},
        threeItemOptions);

    if (!threeItemResult.ok || threeItemResult.data.size() != 3) {
        return 23;
    }

    double threeItemCalories = 0.0;
    QStringList threeItemIds;
    for (const ExercisePlanItem& item : threeItemResult.data) {
        threeItemCalories += item.caloriesBurned;
        threeItemIds.append(item.exerciseId);
        if (item.durationMinutes != 5) {
            return 24;
        }
    }

    if (threeItemIds.removeDuplicates() != 0
        || threeItemCalories + 1e-9 < 105.0
        || threeItemCalories > 115.5 + 1e-9) {
        return 25;
    }

    // 即使数据库正常，只要所有允许组合都达不到目标，也应给出正式失败。
    const auto impossibleResult = recommender.generate(
        validUser,
        1000.0,
        {validExercise, swimmingExercise, cyclingExercise},
        threeItemOptions);

    if (impossibleResult.ok
        || impossibleResult.code
            != QStringLiteral("NO_FEASIBLE_EXERCISE_PLAN")) {
        return 26;
    }

    // NaN 和无穷大 MET 都不能进入热量计算。
    Exercise nanMetExercise = validExercise;
    nanMetExercise.id = QStringLiteral("nan-met");
    nanMetExercise.metValue = std::numeric_limits<double>::quiet_NaN();

    Exercise infiniteMetExercise = validExercise;
    infiniteMetExercise.id = QStringLiteral("infinite-met");
    infiniteMetExercise.metValue =
        std::numeric_limits<double>::infinity();

    const auto nonFiniteMetResult = recommender.generate(
        validUser,
        100.0,
        {nanMetExercise, infiniteMetExercise},
        options);

    if (nonFiniteMetResult.code
        != QStringLiteral("NO_ELIGIBLE_EXERCISE")) {
        return 27;
    }

    // 排除列表两侧可能带有用户输入的空格，比较前应进行规范化。
    UserProfile whitespaceDislikeUser = validUser;
    whitespaceDislikeUser.dislikedExerciseIds.append(
        QStringLiteral("  running  "));

    const auto whitespaceDislikeResult = recommender.generate(
        whitespaceDislikeUser,
        196.0,
        validDatabase,
        options);

    if (whitespaceDislikeResult.code
        != QStringLiteral("NO_ELIGIBLE_EXERCISE")) {
        return 28;
    }

    // 容差为零时，只允许不低于且不高于目标的方案，也就是精确命中。
    ExerciseRecommendationOptions zeroToleranceOptions = options;
    zeroToleranceOptions.upperToleranceRatio = 0.0;

    const auto zeroToleranceExactResult = recommender.generate(
        validUser,
        196.0,
        validDatabase,
        zeroToleranceOptions);
    const auto zeroToleranceMissResult = recommender.generate(
        validUser,
        195.0,
        validDatabase,
        zeroToleranceOptions);

    if (!zeroToleranceExactResult.ok
        || zeroToleranceMissResult.code
            != QStringLiteral("NO_FEASIBLE_EXERCISE_PLAN")) {
        return 29;
    }

    // 离散时长从 minimumDurationMinutes 开始，再按 step 增长。
    // 7、13、19 分钟中，19 分钟跑步消耗 186.2 千卡，能满足 180 千卡。
    ExerciseRecommendationOptions customStepOptions = options;
    customStepOptions.minimumDurationMinutes = 7;
    customStepOptions.durationStepMinutes = 6;
    customStepOptions.maximumDurationMinutesPerExercise = 20;

    const auto customStepResult = recommender.generate(
        validUser,
        180.0,
        validDatabase,
        customStepOptions);

    if (!customStepResult.ok
        || customStepResult.data.size() != 1
        || customStepResult.data.first().durationMinutes != 19
        || std::abs(customStepResult.data.first().caloriesBurned - 186.2)
            > 1e-9) {
        return 30;
    }

    // 多个单项方案都合法时，应选择热量更接近目标的一项。
    Exercise nearTargetExercise;
    nearTargetExercise.id = QStringLiteral("near-target");
    nearTargetExercise.name = QStringLiteral("接近目标的运动");
    nearTargetExercise.metValue = 8.4; // 20 分钟消耗 205.8 千卡。

    const auto bestSingleResult = recommender.generate(
        validUser,
        196.0,
        {nearTargetExercise, validExercise},
        options);

    if (!bestSingleResult.ok
        || bestSingleResult.data.first().exerciseId
            != QStringLiteral("running")
        || std::abs(bestSingleResult.data.first().caloriesBurned - 196.0)
            > 1e-9) {
        return 31;
    }

    // 三种运动目标使用相同热量硬约束，但会在全部合法候选中选择不同的
    // 强度或类别。这里限制为单项运动，便于精确验证目标排序。
    Exercise lightHealthExercise;
    lightHealthExercise.id = QStringLiteral("light-health");
    lightHealthExercise.name = QStringLiteral("轻松步行");
    lightHealthExercise.metValue = 3.0;
    lightHealthExercise.category = ExerciseCategory::Aerobic;

    Exercise buildFitnessExercise;
    buildFitnessExercise.id = QStringLiteral("build-fitness");
    buildFitnessExercise.name = QStringLiteral("中等强度训练");
    buildFitnessExercise.metValue = 5.0;
    buildFitnessExercise.category = ExerciseCategory::Aerobic;

    Exercise muscleGainCardio;
    muscleGainCardio.id = QStringLiteral("high-cardio");
    muscleGainCardio.name = QStringLiteral("较高强度有氧");
    muscleGainCardio.metValue = 6.5;
    muscleGainCardio.category = ExerciseCategory::Aerobic;

    Exercise muscleGainStrength = muscleGainCardio;
    muscleGainStrength.id = QStringLiteral("strength-training");
    muscleGainStrength.name = QStringLiteral("力量训练");
    muscleGainStrength.metValue = 6.5;
    muscleGainStrength.category = ExerciseCategory::Strength;

    const QVector<Exercise> goalDatabase{
        lightHealthExercise,
        buildFitnessExercise,
        muscleGainCardio,
        muscleGainStrength};
    ExerciseRecommendationOptions goalOptions = options;
    goalOptions.maximumExerciseItems = 1;

    UserProfile lightHealthUser = validUser;
    lightHealthUser.exerciseGoal = ExerciseGoal::LightHealth;
    const auto lightHealthResult = recommender.generate(
        lightHealthUser,
        196.0,
        goalDatabase,
        goalOptions);
    if (!lightHealthResult.ok
        || lightHealthResult.data.first().exerciseId
            != QStringLiteral("light-health")) {
        return 32;
    }

    UserProfile buildFitnessUser = validUser;
    buildFitnessUser.exerciseGoal = ExerciseGoal::BuildFitness;
    const auto buildFitnessResult = recommender.generate(
        buildFitnessUser,
        196.0,
        goalDatabase,
        goalOptions);
    if (!buildFitnessResult.ok
        || buildFitnessResult.data.first().exerciseId
            != QStringLiteral("build-fitness")) {
        return 33;
    }

    UserProfile muscleGainUser = validUser;
    muscleGainUser.exerciseGoal = ExerciseGoal::MuscleGain;
    const auto muscleGainResult = recommender.generate(
        muscleGainUser,
        196.0,
        goalDatabase,
        goalOptions);
    if (!muscleGainResult.ok
        || muscleGainResult.data.first().exerciseId
            != QStringLiteral("strength-training")) {
        return 34;
    }

    // 如果某次过滤后没有力量类别，增肌目标仍回退到较高强度候选。
    const auto muscleGainFallbackResult = recommender.generate(
        muscleGainUser,
        196.0,
        {lightHealthExercise, buildFitnessExercise, muscleGainCardio},
        goalOptions);
    if (!muscleGainFallbackResult.ok
        || muscleGainFallbackResult.data.first().exerciseId
            != QStringLiteral("high-cardio")) {
        return 35;
    }

    // 同目标、同 MET、同热量的候选中，历史星级对应的项目权重应打破平局。
    Exercise feedbackLow = buildFitnessExercise;
    feedbackLow.id = QStringLiteral("feedback-low");
    Exercise feedbackHigh = buildFitnessExercise;
    feedbackHigh.id = QStringLiteral("feedback-high");

    ExerciseRecommendationOptions feedbackOptions = goalOptions;
    feedbackOptions.preference.itemWeights.insert(
        feedbackLow.id,
        *feedbackWeightFromStars(1));
    feedbackOptions.preference.itemWeights.insert(
        feedbackHigh.id,
        *feedbackWeightFromStars(5));
    const auto feedbackResult = recommender.generate(
        buildFitnessUser,
        196.0,
        {feedbackLow, feedbackHigh},
        feedbackOptions);
    if (!feedbackResult.ok
        || feedbackResult.data.first().exerciseId
            != QStringLiteral("feedback-high")) {
        return 36;
    }

    ExerciseRecommendationOptions invalidPreferenceOptions = goalOptions;
    invalidPreferenceOptions.preference.itemWeights.insert(
        QStringLiteral("invalid-weight"),
        std::numeric_limits<double>::quiet_NaN());
    if (recommender.generate(
            buildFitnessUser,
            196.0,
            goalDatabase,
            invalidPreferenceOptions).code
        != QStringLiteral("INVALID_OPTIONS")) {
        return 37;
    }

    return 0;
}
