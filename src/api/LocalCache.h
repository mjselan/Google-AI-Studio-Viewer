#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSet>

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
    QSet<QString> getAllCachedFileIds();
    void deleteConversation(const QString &fileId);

    // --- Search methods ---
    QSet<QString> searchContent(const QString &term);
    QSet<QString> findConversationsByAttachmentId(const QString &attachmentId);

private:
    bool m_initialized = false;
    QSqlDatabase m_db;
};