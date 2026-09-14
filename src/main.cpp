#include <QApplication>
#include <QIcon>
#include <QStandardPaths>
#include <QDir>
#include "app/Application.h"
#include "app/Logger.h"
#include "ui/MainWindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h> // <-- Add this header
#pragma comment(lib, "Shell32.lib")
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // Ensures Windows 10/11 taskbar displays your custom icon rather than a generic host
    SetCurrentProcessExplicitAppUserModelID(L"GoogleAIViewer.App");
#endif

    Application app(argc, argv);
    app.init();

    // Set the application-wide icon from the embedded Qt resource (Works on Win, Mac, Linux!)
    app.setWindowIcon(QIcon(":/icon_512.png"));

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