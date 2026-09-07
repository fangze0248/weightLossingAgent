#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/Exercise.h"
#include "../models/PlanModels.h"
#include "../models/RecommendationPreference.h"
#include "../models/UserProfile.h"

#include <QHash>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <optional>

struct ExerciseRecommendationOptions {
    QStringList excludedExerciseIds;
    int durationStepMinutes = 5;
    int minimumDurationMinutes = 5;
    int maximumDurationMinutesPerExercise = 120;
    int maximumExerciseItems = 3;
    double upperToleranceRatio = 0.10;
    std::optional<quint32> randomSeed;
    RecommendationPreference preference;
    // 追加在末尾以兼容已有的按位置聚合初始化调用。
    // 近期不同周计划中的运动 ID -> 暴露惩罚，属于软约束，
    // 候选不足时仍允许重新推荐。
    QHash<QString, double> recentExercisePenalties;
};

class IExerciseRecommender {
public:
    virtual ~IExerciseRecommender() = default;

    virtual ServiceResult<QVector<ExercisePlanItem>> generate(
        const UserProfile& user,
        double targetCalories,
        const QVector<Exercise>& exerciseDatabase,
        const ExerciseRecommendationOptions& options = {}) const = 0;
};
