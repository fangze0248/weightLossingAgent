#pragma once

#include "interfaces/IDataExchangeService.h"

class CsvDataExchangeService final : public IDataExchangeService
{
public:
    ServiceResult<ImportBatch<UserProfile>> importUsers(
        const QString& filePath,
        DataFormat format) const override;
    ServiceResult<ImportBatch<Exercise>> importExercises(
        const QString& filePath,
        DataFormat format) const override;
    ServiceResult<ImportBatch<Recipe>> importRecipes(
        const QString& filePath,
        DataFormat format) const override;
    ServiceResult<ImportStreamSummary> streamRecipes(
        const QString& filePath,
        DataFormat format,
        const std::function<void(const Recipe&)>& visitor) const override;

    ServiceResult<bool> exportUsers(
        const QVector<UserProfile>& users,
        const QString& filePath,
        DataFormat format) const override;
    ServiceResult<bool> exportExercises(
        const QVector<Exercise>& exercises,
        const QString& filePath,
        DataFormat format) const override;
    ServiceResult<bool> exportRecipes(
        const QVector<Recipe>& recipes,
        const QString& filePath,
        DataFormat format) const override;
    ServiceResult<bool> exportWeeklyPlan(
        const WeeklyPlan& plan,
        const QString& filePath,
        DataFormat format) const override;
};
