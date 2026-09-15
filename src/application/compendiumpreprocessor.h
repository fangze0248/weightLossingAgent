#pragma once

#include "contracts/ServiceResult.h"

#include <QString>

class IDataExchangeService;

struct CompendiumPreprocessOptions {
    int maximumExercisesPerCategory = 150;
    double minimumMet = 2.0;
    double maximumMet = 18.0;
};

struct CompendiumPreprocessSummary {
    int parsedRows = 0;
    int malformedRows = 0;
    int filteredRows = 0;
    int selectedRows = 0;
    QString outputPath;
};

class CompendiumPreprocessor
{
public:
    explicit CompendiumPreprocessor(
        const IDataExchangeService& dataExchangeService);

    ServiceResult<CompendiumPreprocessSummary> preprocess(
        const QString& inputPath,
        const QString& outputPath,
        const CompendiumPreprocessOptions& options = {}) const;

private:
    const IDataExchangeService& dataExchangeService_;
};
