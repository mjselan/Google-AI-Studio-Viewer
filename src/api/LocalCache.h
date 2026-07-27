#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QSqlDatabase>

class LocalCache
{
public:
    LocalCache();
    ~LocalCache();

    void init();
    void saveConversation(const QString &fileId, const QString &fileName,
                          const QDateTime &modTime, const QJsonObject &data);
    QJsonObject loadConversation(const QString &fileId);
    QDateTime getModifiedTime(const QString &fileId);

private:
    bool m_initialized = false;
    QSqlDatabase m_db;
};
