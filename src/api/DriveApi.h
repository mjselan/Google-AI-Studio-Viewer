#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QList>
#include <QJsonObject>
#include <QNetworkReply>
#include <QDateTime>
#include <functional>


struct DriveFile
{
    QString fileId;
    QString fileName;
    QDateTime modifiedTime;
    QString mimeType;
    QString fileExtension;

    // Default constructor for empty stubs
    DriveFile() = default;

    // Constructor to parse from API response
    explicit DriveFile(const QJsonObject &obj);

    QString info();
};

class DriveApi : public QObject
{
    Q_OBJECT
public:
    explicit DriveApi(QObject *parent = nullptr);

    void setAccessToken(const QString &token);

    void findFolder(const QString &folderName);
    void listConversations(const QString &folderId);
    void downloadConversation(const QString &fileId, const QString &fileName);
    void downloadAttachment(const QString &fileId);
    void searchConversations(const QString &query, bool includeAttachments = false);
    void retryPendingRequests();
    static bool isIgnoredAttachment(const QString &mime, const QString &ext);
    void findParentConversation(const QString &attachmentFileId);

signals:
    void folderFound(const QString &folderId);
    void folderNotFound();
    void conversationListReady(const QList<DriveFile> &files);
    void searchResultsReady(const QList<DriveFile> &results);
    void conversationDownloaded(const QString &fileId, const QString &fileName, const QJsonObject &data);
    void attachmentDownloaded(const QString &fileId, const QByteArray &data, const QString &mimeType);
    void apiError(const QString &error);
    void tokenNeedsRefresh();
    void parentConversationFound(const QString &attachmentId, const DriveFile &parent);
    void parentConversationNotFound(const QString &attachmentId);

private:
    struct PendingRequest
    {
        QString url;
        std::function<void(const QJsonObject &)> callback;
        bool binary = false;
    };

    void sendGet(const QString &url, std::function<void(const QJsonObject &)> callback);
    void handleReply(QNetworkReply *reply, std::function<void(const QJsonObject &)> callback);

    QNetworkAccessManager *m_net;
    QString m_token;
    QString m_folderId;
    QList<PendingRequest> m_pendingRequests;
    bool m_refreshInProgress = false;
};