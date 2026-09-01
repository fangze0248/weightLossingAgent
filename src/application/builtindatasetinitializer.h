#pragma once

#include "contracts/ServiceResult.h"

#include <QSqlDatabase>
#include <QString>

class IDataExchangeService;
class IExerciseRepository;
class IRecipeRepository;

struct DatasetInitializationSummary {
    bool imported = false;
    int parsedRows = 0;
    int storedRows = 0;
    int skippedRows = 0;
    QString contentHash;
};

class BuiltinDatasetInitializer
{
public:
    BuiltinDatasetInitializer(
        QSqlDatabase database,
        IRecipeRepository& recipeRepository,
        const IDataExchangeService& dataExchangeService);
    BuiltinDatasetInitializer(
        QSqlDatabase database,
        IExerciseRepository& exerciseRepository,
        IRecipeRepository& recipeRepository,
        const IDataExchangeService& dataExchangeService);

    ServiceResult<DatasetInitializationSummary> importExercisesIfChanged(
        const QString& datasetKey,
        const QString& datasetPath);

    ServiceResult<DatasetInitializationSummary> importRecipesIfChanged(
        const QString& datasetKey,
        const QString& datasetPath);

private:
    QSqlDatabase database_;
    IExerciseRepository* exerciseRepository_ = nullptr;
    IRecipeRepository* recipeRepository_ = nullptr;
    const IDataExchangeService& dataExchangeService_;
};
