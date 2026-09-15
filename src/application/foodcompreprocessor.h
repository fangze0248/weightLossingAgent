#pragma once

#include "contracts/ServiceResult.h"

#include <QString>

class IDataExchangeService;

struct FoodComPreprocessOptions {
    int maximumRecipesPerMeal = 250;
    double minimumCalories = 100.0;
    double maximumCalories = 1000.0;
    int minimumIngredients = 2;
    int maximumIngredients = 25;
    double maximumSodiumMg = 2500.0;
};

struct FoodComPreprocessSummary {
    int parsedRows = 0;
    int malformedRows = 0;
    int filteredRows = 0;
    int selectedRows = 0;
    QString outputPath;
};

class FoodComPreprocessor
{
public:
    explicit FoodComPreprocessor(
        const IDataExchangeService& dataExchangeService);

    ServiceResult<FoodComPreprocessSummary> preprocess(
        const QString& inputPath,
        const QString& outputPath,
        const FoodComPreprocessOptions& options = {}) const;

private:
    const IDataExchangeService& dataExchangeService_;
};
