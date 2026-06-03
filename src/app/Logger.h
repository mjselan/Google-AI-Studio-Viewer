#pragma once

#include <QString>
#include <QFile>
#include <QMutex>
#include <QDateTime>

class Logger
{
public:
    enum Level { Debug, Info, Warning, Error };

    static Logger &instance();

    void init(const QString &filePath);
    void log(Level level, const QString &message);
    void debug(const QString &message) { log(Debug, message); }
    void info(const QString &message)  { log(Info, message); }
    void warn(const QString &message)  { log(Warning, message); }
    void error(const QString &message) { log(Error, message); }

    static QString levelName(Level level);

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    QFile m_file;
    QMutex m_mutex;
};
