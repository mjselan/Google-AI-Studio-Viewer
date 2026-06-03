#include <QApplication>
#include "app/Application.h"
#include "app/Logger.h"
#include "ui/MainWindow.h"
#include <QStandardPaths>

int main(int argc, char *argv[])
{
    Application app(argc, argv);
    app.init();

    QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + "/GoogleAIViewer.log";
    Logger::instance().init(logPath);
    Logger::instance().info("=== Google AI Studio Viewer started ===");

    MainWindow window;
    window.setWindowTitle("Google AI Studio Viewer");
    window.resize(1200, 800);
    window.show();

    int ret = app.exec();
    Logger::instance().info(QString("=== App exited with code %1 ===").arg(ret));
    return ret;
}
