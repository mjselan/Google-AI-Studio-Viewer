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

    static const QString connectionName = QStringLiteral("ViewerCacheConnection");
    if (QSqlDatabase::contains(connectionName))
    {
        QSqlDatabase existing = QSqlDatabase::database(connectionName, false);
        if (!existing.isOpen())
            QSqlDatabase::removeDatabase(connectionName);
    }
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open())
    {
        Logger::instance().error("Failed to open SQLite database: " + m_db.lastError().text());
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
        Logger::instance().error("Failed to create table in cache: " + query.lastError().text());
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
    {
        Logger::instance().error(
            QString("Cache save failed for '%1': %2").arg(fileId, query.lastError().text()));
    }
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

QSet<QString> LocalCache::getAllCachedFileIds()
{
    QSet<QString> ids;
    if (!m_initialized) return ids;

    QSqlQuery query("SELECT file_id FROM conversations", m_db);
    while (query.next()) {
        ids.insert(query.value(0).toString());
    }
    return ids;
}

void LocalCache::deleteConversation(const QString &fileId)
{
    if (!m_initialized) return;

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM conversations WHERE file_id = :fid");
    query.bindValue(":fid", fileId);
    query.exec();
}

QSet<QString> LocalCache::searchContent(const QString &term)
{
    QSet<QString> matches;
    if (!m_initialized || term.trimmed().isEmpty())
        return matches;

    QSqlQuery q(m_db);
    // Searches both conversation title and full message JSON (prompts, answers, thoughts)
    q.prepare("SELECT file_id FROM conversations WHERE file_name LIKE :term OR json_data LIKE :term");
    q.bindValue(":term", "%" + term + "%");

    if (q.exec())
    {
        while (q.next())
        {
            matches.insert(q.value(0).toString());
        }
    }
    else
    {
        Logger::instance().error("LocalCache::searchContent failed: " + q.lastError().text());
    }
    return matches;
}

QSet<QString> LocalCache::findConversationsByAttachmentId(const QString &attachmentId)
{
    QSet<QString> matches;
    if (!m_initialized || attachmentId.trimmed().isEmpty())
        return matches;

    QSqlQuery q(m_db);
    // Finds any conversation whose JSON references this attachment ID
    q.prepare("SELECT file_id FROM conversations WHERE json_data LIKE :attId");
    q.bindValue(":attId", "%" + attachmentId + "%");

    if (q.exec())
    {
        while (q.next())
        {
            matches.insert(q.value(0).toString());
        }
    }
    else
    {
        Logger::instance().error("LocalCache::findConversationsByAttachmentId failed: " + q.lastError().text());
    }
    return matches;
}