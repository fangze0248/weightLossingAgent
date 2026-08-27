#pragma once

#include <QSqlDatabase>
#include <QString>

class DatabaseManager
{
public:
    explicit DatabaseManager(QString databasePath = defaultDatabasePath());
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    static QString defaultDatabasePath();

    bool open(QString* errorMessage = nullptr);
    bool initialize(QString* errorMessage = nullptr);
    bool seedDemoData(QString* errorMessage = nullptr);

    bool isOpen() const;
    QString databasePath() const;
    QSqlDatabase database() const;

private:
    static QString connectionName();
    bool executeStatement(const QString& sql, QString* errorMessage);

    QString databasePath_;
    QSqlDatabase database_;
};
