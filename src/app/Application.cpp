#include "Application.h"

Application::Application(int &argc, char **argv)
    : QApplication(argc, argv)
    , m_settings("GoogleAIViewer", "GoogleAIViewer")
{
}

void Application::init()
{
    setApplicationName("Google AI Studio Viewer");
    setApplicationVersion("1.0.0");
    setOrganizationName("GoogleAIViewer");
}

QSettings &Application::settings()
{
    return m_settings;
}
