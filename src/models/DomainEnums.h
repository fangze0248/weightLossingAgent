#pragma once

#include <QString>
#include <optional>

enum class Gender {
    Male,
    Female
};

enum class GoalType {
    Lose,
    Maintain,
    Gain
};

// 运动偏好目标与体重目标分开：GoalType 决定能量方向，ExerciseGoal
// 决定合法运动方案之间的排序偏好。
enum class ExerciseGoal {
    LightHealth,
    BuildFitness,
    MuscleGain
};

enum class ExerciseCategory {
    Aerobic,
    Strength,
    Flexibility,
    Balance,
    Other
};

enum class MealType {
    Breakfast,
    Lunch,
    Dinner,
    Snack
};

enum class FeedbackRating {
    Dislike = -1,
    Neutral = 0,
    Like = 1
};

enum class RecommendationItemType {
    Exercise,
    Recipe
};

enum class DataFormat {
    Json,
    Csv
};

inline QString toStorageString(Gender value)
{
    return value == Gender::Male ? QStringLiteral("M") : QStringLiteral("F");
}

inline QString toStorageString(GoalType value)
{
    switch (value) {
    case GoalType::Lose: return QStringLiteral("lose");
    case GoalType::Maintain: return QStringLiteral("maintain");
    case GoalType::Gain: return QStringLiteral("gain");
    }
    return QStringLiteral("lose");
}

inline QString toStorageString(ExerciseGoal value)
{
    switch (value) {
    case ExerciseGoal::LightHealth:
        return QStringLiteral("light_health");
    case ExerciseGoal::BuildFitness:
        return QStringLiteral("build_fitness");
    case ExerciseGoal::MuscleGain:
        return QStringLiteral("muscle_gain");
    }
    return QStringLiteral("light_health");
}

inline QString toStorageString(ExerciseCategory value)
{
    switch (value) {
    case ExerciseCategory::Aerobic: return QStringLiteral("aerobic");
    case ExerciseCategory::Strength: return QStringLiteral("strength");
    case ExerciseCategory::Flexibility: return QStringLiteral("flexibility");
    case ExerciseCategory::Balance: return QStringLiteral("balance");
    case ExerciseCategory::Other: return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

inline QString toStorageString(MealType value)
{
    switch (value) {
    case MealType::Breakfast: return QStringLiteral("breakfast");
    case MealType::Lunch: return QStringLiteral("lunch");
    case MealType::Dinner: return QStringLiteral("dinner");
    case MealType::Snack: return QStringLiteral("snack");
    }
    return QStringLiteral("snack");
}

inline std::optional<Gender> genderFromStorageString(const QString& value)
{
    const QString normalized = value.trimmed().toUpper();
    if (normalized == QStringLiteral("M") || normalized == QStringLiteral("MALE")) {
        return Gender::Male;
    }
    if (normalized == QStringLiteral("F") || normalized == QStringLiteral("FEMALE")) {
        return Gender::Female;
    }
    return std::nullopt;
}

inline std::optional<GoalType> goalTypeFromStorageString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("lose")) return GoalType::Lose;
    if (normalized == QStringLiteral("maintain")) return GoalType::Maintain;
    if (normalized == QStringLiteral("gain")) return GoalType::Gain;
    return std::nullopt;
}

inline std::optional<ExerciseGoal> exerciseGoalFromStorageString(
    const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("light_health")) {
        return ExerciseGoal::LightHealth;
    }
    if (normalized == QStringLiteral("build_fitness")) {
        return ExerciseGoal::BuildFitness;
    }
    if (normalized == QStringLiteral("muscle_gain")) {
        return ExerciseGoal::MuscleGain;
    }
    return std::nullopt;
}

inline std::optional<MealType> mealTypeFromStorageString(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("breakfast")) return MealType::Breakfast;
    if (normalized == QStringLiteral("lunch")) return MealType::Lunch;
    if (normalized == QStringLiteral("dinner")) return MealType::Dinner;
    if (normalized == QStringLiteral("snack")) return MealType::Snack;
    return std::nullopt;
}
