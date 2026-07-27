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

    void retryPendingRequests();

signals:
    void folderFound(const QString &folderId);
    void folderNotFound();
    void conversationListReady(const QList<DriveFile> &files);
    void conversationDownloaded(const QString &fileId, const QString &fileName, const QJsonObject &data);
    void attachmentDownloaded(const QString &fileId, const QByteArray &data, const QString &mimeType);
    void apiError(const QString &error);
    void tokenNeedsRefresh();

private:
    struct PendingRequest
    {
        QString url;
        std::function<void(const QJsonObject &)> callback;
    };

    void sendGet(const QString &url, std::function<void(const QJsonObject &)> callback);
    void handleReply(QNetworkReply *reply, std::function<void(const QJsonObject &)> callback);

    QNetworkAccessManager *m_net;
    QString m_token;
    QList<PendingRequest> m_pendingRequests;
};
