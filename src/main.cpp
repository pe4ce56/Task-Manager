#include "mainwindow.h"

#include <QApplication>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("TaskManager"));
    QCoreApplication::setApplicationName(QStringLiteral("TaskManager"));
    const QString dataFile = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/tasks.json");
    MainWindow window(dataFile);
    window.show();
    return application.exec();
}
