#pragma once

#include <QApplication>
#include <QSettings>

class Application : public QApplication
{
    Q_OBJECT
public:
    Application(int &argc, char **argv);
    void init();

    QSettings &settings();

private:
    QSettings m_settings;
};
