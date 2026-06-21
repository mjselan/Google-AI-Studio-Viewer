#include "Logger.h"
#include <QDir>
#include <QTextStream>
#include <QMutexLocker>
#include <iostream>
#include <QApplication>

//#define NO_LOGGING

Logger::~Logger()
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen())
        m_file.close();
}

Logger &Logger::instance()
{
    static Logger s;
    return s;
}

void Logger::init(const QString &filePath)
{

#ifdef NO_LOGGING
    return;
#endif
    QMutexLocker lock(&m_mutex);
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    if (m_file.isOpen())
        m_file.close();

    m_file.setFileName(filePath);
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void Logger::log(Level level, const QString &message)
{
#ifdef NO_LOGGING
    return;
#endif
    QMutexLocker lock(&m_mutex);

    QString line = QString("[%1] [%2] %3\n")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
        .arg(levelName(level))
        .arg(message);

    if (m_file.isOpen())
    {
        m_file.write(line.toUtf8());
        m_file.flush();
    }

    // Also print to stderr for immediate visibility
    std::cerr << line.toStdString();
}

QString Logger::levelName(Level level)
{
    switch (level)
    {
    case Debug:   return "DEBUG";
    case Info:    return "INFO";
    case Warning: return "WARN";
    case Error:   return "ERROR";
    }
    return "UNKNOWN";
}
