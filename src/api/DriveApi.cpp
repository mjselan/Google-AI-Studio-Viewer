#include "DriveApi.h"
#include "app/Logger.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDebug>

DriveApi::DriveApi(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

void DriveApi::setAccessToken(const QString &token)
{
    m_token = token;
}

void DriveApi::sendGet(const QString &urlStr, std::function<void(const QJsonObject &)> callback)
{
    QUrl qurl(urlStr);
    QNetworkRequest req(qurl);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    auto *reply = m_net->get(req);
    handleReply(reply, callback);
}

void DriveApi::handleReply(QNetworkReply *reply, std::function<void(const QJsonObject &)> callback)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]()
    {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status == 401)
        {
            Logger::instance().warn(
                QString("HTTP 401 on %1 — queuing for token refresh")
                    .arg(reply->url().toString()));
            PendingRequest pending;
            pending.url = reply->url().toString();
            pending.callback = callback;
            m_pendingRequests.append(pending);
            emit tokenNeedsRefresh();
            return;
        }

        if (reply->error() != QNetworkReply::NoError)
        {
            qWarning() << "Drive API error:" << reply->errorString();
            return;
        }

        auto doc = QJsonDocument::fromJson(reply->readAll());
        callback(doc.object());
    });
}

void DriveApi::retryPendingRequests()
{
    int count = m_pendingRequests.size();
    Logger::instance().info(QString("Retrying %1 pending requests with new token").arg(count));
    QList<PendingRequest> retries = m_pendingRequests;
    m_pendingRequests.clear();
    for (const auto &pending : retries)
        sendGet(pending.url, pending.callback);
}

void DriveApi::findFolder(const QString &folderName)
{
    QUrlQuery query;
    query.addQueryItem("q", QString("name='%1' and mimeType='application/vnd.google-apps.folder' and trashed=false").arg(folderName));
    query.addQueryItem("fields", "files(id,name)");
    query.addQueryItem("pageSize", "10");

    QUrl url("https://www.googleapis.com/drive/v3/files");
    url.setQuery(query);

    sendGet(url.toString(), [this](const QJsonObject &response)
    {
        auto files = response["files"].toArray();
        if (files.isEmpty())
            emit folderNotFound();
        else
            emit folderFound(files.first().toObject()["id"].toString());
    });
}

void DriveApi::listConversations(const QString &folderId)
{
    QUrlQuery query;
    query.addQueryItem("q", QString("'%1' in parents and trashed=false").arg(folderId));
    query.addQueryItem("fields", "files(id,name,modifiedTime)");
    query.addQueryItem("orderBy", "modifiedTime desc");
    query.addQueryItem("pageSize", "100");

    QUrl url("https://www.googleapis.com/drive/v3/files");
    url.setQuery(query);

    sendGet(url.toString(), [this](const QJsonObject &response)
    {
        QList<QPair<QString, QString>> files;
        for (const auto &f : response["files"].toArray())
        {
            auto obj = f.toObject();
            files.append({obj["id"].toString(), obj["name"].toString()});
        }
        emit conversationListReady(files);
    });
}

void DriveApi::downloadConversation(const QString &fileId, const QString &fileName)
{
    QUrl url(QString("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(fileId));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    auto *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId, fileName]()
    {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401)
        {
            Logger::instance().warn(
                QString("HTTP 401 downloading '%1' (%2) — queuing for retry")
                    .arg(fileName).arg(fileId));
            PendingRequest pending;
            pending.url = reply->url().toString();
            pending.callback = [this, fileId, fileName](const QJsonObject &data)
            {
                emit conversationDownloaded(fileId, fileName, data);
            };
            m_pendingRequests.append(pending);
            emit tokenNeedsRefresh();
            return;
        }

        if (reply->error() != QNetworkReply::NoError)
        {
            Logger::instance().error(
                QString("Download failed: '%1' (%2) — %3")
                    .arg(fileName, fileId, reply->errorString()));
            emit apiError("Download failed: " + reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        Logger::instance().debug(
            QString("Downloaded conversation: '%1' (%2)").arg(fileName, fileId));
        emit conversationDownloaded(fileId, fileName, doc.object());
    });
}

void DriveApi::downloadAttachment(const QString &fileId)
{
    QUrl url(QString("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(fileId));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    auto *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, fileId]()
    {
        reply->deleteLater();

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 401)
        {
            Logger::instance().warn(
                QString("HTTP 401 downloading attachment %1 — queuing for retry").arg(fileId));
            PendingRequest pending;
            pending.url = reply->url().toString();
            pending.callback = [this, fileId](const QJsonObject &)
            {
                downloadAttachment(fileId);
            };
            m_pendingRequests.append(pending);
            emit tokenNeedsRefresh();
            return;
        }

        if (reply->error() != QNetworkReply::NoError)
        {
            Logger::instance().error(
                QString("Attachment download failed: %1 — %2")
                    .arg(fileId, reply->errorString()));
            emit apiError("Attachment download failed: " + reply->errorString());
            return;
        }
        QString mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        QByteArray data = reply->readAll();
        Logger::instance().debug(
            QString("Attachment downloaded: %1 (%2, %3 bytes)")
                .arg(fileId, mime).arg(data.size()));
        emit attachmentDownloaded(fileId, data, mime);
    });
}
