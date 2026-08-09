#include <QApplication>
#include "app/Application.h"
#include "app/Logger.h"
#include "ui/MainWindow.h"
#include <QDir>

int main(int argc, char *argv[])
{
    Application app(argc, argv);
    app.init();

    QString logPath = QDir(QCoreApplication::applicationDirPath())
                          .filePath("GoogleAIViewer.log");
    Logger::instance().init(logPath);

    MainWindow window;
    window.setWindowTitle("Google AI Studio Viewer");
    window.resize(1200, 800);
    window.show();

    return app.exec();
}