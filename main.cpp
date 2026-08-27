#include "mainwindow.h"
#include "database/databasemanager.h"

#include <QApplication>
#include <QMessageBox>

#include "recommendation/recommendationcore.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SummerSchool"));
    QCoreApplication::setApplicationName(QStringLiteral("WeightLossingAgent"));

    DatabaseManager databaseManager;
    QString databaseError;
    if (!databaseManager.open(&databaseError)
        || !databaseManager.initialize(&databaseError)
        || !databaseManager.seedDemoData(&databaseError)) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("Database error"),
            databaseError);
        return 1;
    }

    MainWindow w;
    w.show();
    return QApplication::exec();
}
