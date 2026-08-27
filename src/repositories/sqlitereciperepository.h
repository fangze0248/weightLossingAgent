#pragma once

#include "interfaces/IRecipeRepository.h"

#include <QSqlDatabase>

class SqliteRecipeRepository final : public IRecipeRepository
{
public:
    explicit SqliteRecipeRepository(QSqlDatabase database);

    ServiceResult<QVector<Recipe>> findAll(
        const RecipeFilter& filter = {}) const override;
    ServiceResult<std::optional<Recipe>> findById(
        const QString& id) const override;
    ServiceResult<Recipe> add(const Recipe& recipe) override;
    ServiceResult<Recipe> update(const Recipe& recipe) override;
    ServiceResult<bool> remove(const QString& id) override;

private:
    QSqlDatabase database_;
};
