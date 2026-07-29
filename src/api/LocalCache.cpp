#include "LocalCache.h"
#include "app/Logger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>

LocalCache::LocalCache() {}

LocalCache::~LocalCache() {}

void LocalCache::init()
{
    if (m_initialized)
        return;

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QString dbPath = dataDir + "/viewer_cache.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open())
    {
        return;
    }

    QSqlQuery query(m_db);
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS conversations ("
            "file_id TEXT PRIMARY KEY, "
            "file_name TEXT, "
            "modified_time TEXT, "
            "json_data TEXT)"))
    {
        return;
    }

    m_initialized = true;
}

void LocalCache::saveConversation(const QString &fileId, const QString &fileName,
                                  const QDateTime &modTime, const QJsonObject &data)
{
    if (!m_initialized)
        return;

    QSqlQuery query(m_db);
    query.prepare(
        "INSERT OR REPLACE INTO conversations (file_id, file_name, modified_time, json_data) "
        "VALUES (:fid, :fname, :mtime, :jdata)");
    query.bindValue(":fid", fileId);
    query.bindValue(":fname", fileName);
    query.bindValue(":mtime", modTime.isValid() ? modTime.toString(Qt::ISODate) : QString());
    query.bindValue(":jdata", QString(QJsonDocument(data).toJson(QJsonDocument::Compact)));

    if (!query.exec())
        return;
}

QJsonObject LocalCache::loadConversation(const QString &fileId)
{
    if (!m_initialized)
        return {};

    QSqlQuery query(m_db);
    query.prepare("SELECT json_data FROM conversations WHERE file_id = :fid");
    query.bindValue(":fid", fileId);

    if (!query.exec() || !query.next())
        return {};

    QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toByteArray());
    return doc.object();
}

QDateTime LocalCache::getModifiedTime(const QString &fileId)
{
    if (!m_initialized)
        return {};

    QSqlQuery query(m_db);
    query.prepare("SELECT modified_time FROM conversations WHERE file_id = :fid");
    query.bindValue(":fid", fileId);

    if (!query.exec() || !query.next())
        return {};

    return QDateTime::fromString(query.value(0).toString(), Qt::ISODate);
}
