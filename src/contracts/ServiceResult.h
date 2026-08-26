#pragma once

#include <QString>
#include <QStringList>
#include <utility>

template<typename T>
struct ServiceResult {
    bool ok = false;
    T data{};
    QString code = QStringLiteral("UNINITIALIZED");
    QString message;
    QStringList warnings;

    static ServiceResult<T> success(
        T value,
        QString successMessage = {},
        QStringList resultWarnings = {})
    {
        ServiceResult<T> result;
        result.ok = true;
        result.data = std::move(value);
        result.code = QStringLiteral("OK");
        result.message = std::move(successMessage);
        result.warnings = std::move(resultWarnings);
        return result;
    }

    static ServiceResult<T> failure(
        QString errorCode,
        QString errorMessage,
        QStringList resultWarnings = {})
    {
        ServiceResult<T> result;
        result.ok = false;
        result.code = std::move(errorCode);
        result.message = std::move(errorMessage);
        result.warnings = std::move(resultWarnings);
        return result;
    }
};
