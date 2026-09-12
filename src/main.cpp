#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <iostream>
#include "app/Application.h"
#include "app/Logger.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    Application app(argc, argv);
    app.init();

    // Store logs in AppData (e.g. C:/Users/<user>/AppData/Roaming/GoogleAIViewer/)
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QString logPath = QDir(dataDir).filePath("GoogleAIViewer.log");

    Logger::instance().init(logPath);

    MainWindow window;
    window.setWindowTitle("Google AI Studio Viewer");
    window.resize(1200, 800);
    window.show();

    return app.exec();
}