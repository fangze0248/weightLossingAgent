#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/Exercise.h"

#include <QString>
#include <QVector>
#include <optional>

struct ExerciseFilter {
    QString keyword;
    std::optional<ExerciseCategory> category;
    std::optional<double> minimumMet;
    std::optional<double> maximumMet;
};

class IExerciseRepository {
public:
    virtual ~IExerciseRepository() = default;

    virtual ServiceResult<QVector<Exercise>> findAll(
        const ExerciseFilter& filter = {}) const = 0;
    virtual ServiceResult<std::optional<Exercise>> findById(
        const QString& id) const = 0;
    virtual ServiceResult<Exercise> add(const Exercise& exercise) = 0;
    virtual ServiceResult<Exercise> update(const Exercise& exercise) = 0;
    virtual ServiceResult<bool> remove(const QString& id) = 0;
};
