#pragma once

#include "interfaces/IExerciseRepository.h"

#include <QSqlDatabase>

class SqliteExerciseRepository final : public IExerciseRepository
{
public:
    explicit SqliteExerciseRepository(QSqlDatabase database);

    ServiceResult<QVector<Exercise>> findAll(
        const ExerciseFilter& filter = {}) const override;
    ServiceResult<std::optional<Exercise>> findById(
        const QString& id) const override;
    ServiceResult<Exercise> add(const Exercise& exercise) override;
    ServiceResult<Exercise> update(const Exercise& exercise) override;
    ServiceResult<bool> remove(const QString& id) override;

private:
    QSqlDatabase database_;
};
