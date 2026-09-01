#pragma once

#include "DomainEnums.h"

#include <QString>
#include <QStringList>

struct UserProfile {
    QString id;
    QString name;
    Gender gender = Gender::Male;
    int age = 0;
    double heightCm = 0.0;
    double weightKg = 0.0;
    double targetWeightKg = 0.0;
    // Past-seven-day average. It is used only to estimate ordinary daily
    // movement; planned workouts are calculated separately.
    int averageDailySteps = 4000;

    // Kept temporarily so existing SQLite databases remain compatible.
    // HealthCalculator no longer uses this value.
    int activityLevel = 1;
    GoalType goalType = GoalType::Lose;
    ExerciseGoal exerciseGoal = ExerciseGoal::LightHealth;

    // Magnitude of the weekly goal. For weight loss, allowed UI values are
    // normally 0.5, 1.0, and 1.5 kg.
    double weeklyGoalKg = 0.5;

    // Portion of the energy deficit assigned to diet; 0.7 means diet:exercise
    // is 7:3.
    double dietContributionRatio = 0.7;

    QStringList dislikedExerciseIds;
    QStringList dislikedRecipeIds;
};
