#pragma once

#include "../contracts/ServiceResult.h"
#include "../models/DomainEnums.h"
#include "../models/Exercise.h"
#include "../models/PlanModels.h"
#include "../models/Recipe.h"
#include "../models/UserProfile.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

template<typename T>
struct ImportBatch {
    QVector<T> items;
    int importedRows = 0;
    int skippedRows = 0;
    QStringList rowMessages;
};

struct ImportStreamSummary {
    int importedRows = 0;
    int skippedRows = 0;
    QStringList rowMessages;
};

class IDataExchangeService {
public:
    virtual ~IDataExchangeService() = default;

    virtual ServiceResult<ImportBatch<UserProfile>> importUsers(
        const QString& filePath,
        DataFormat format) const = 0;
    virtual ServiceResult<ImportBatch<Exercise>> importExercises(
        const QString& filePath,
        DataFormat format) const = 0;
    virtual ServiceResult<ImportBatch<Recipe>> importRecipes(
        const QString& filePath,
        DataFormat format) const = 0;
    virtual ServiceResult<ImportStreamSummary> streamRecipes(
        const QString& filePath,
        DataFormat format,
        const std::function<void(const Recipe&)>& visitor) const = 0;

    virtual ServiceResult<bool> exportUsers(
        const QVector<UserProfile>& users,
        const QString& filePath,
        DataFormat format) const = 0;
    virtual ServiceResult<bool> exportExercises(
        const QVector<Exercise>& exercises,
        const QString& filePath,
        DataFormat format) const = 0;
    virtual ServiceResult<bool> exportRecipes(
        const QVector<Recipe>& recipes,
        const QString& filePath,
        DataFormat format) const = 0;
    virtual ServiceResult<bool> exportWeeklyPlan(
        const WeeklyPlan& plan,
        const QString& filePath,
        DataFormat format) const = 0;
};
