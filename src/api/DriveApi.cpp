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

// Escape single quotes in values embedded in Drive query strings.
static QString escapeQueryValue(const QString &value)
{
    QString escaped = value;
    escaped.replace('\'', "\\'");
    return escaped;
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
    QList<PendingRequest> retries = m_pendingRequests;
    m_pendingRequests.clear();
    for (const auto &pending : retries)
    {
        if (pending.binary)
        {
            // Binary payloads (attachments) would be corrupted by sendGet's
            // JSON parsing; the callback re-issues the raw download itself.
            pending.callback(QJsonObject());
        }
        else
        {
            sendGet(pending.url, pending.callback);
        }
    }
}

void DriveApi::findFolder(const QString &folderName)
{
    QUrlQuery query;
    query.addQueryItem("q", QString("name='%1' and mimeType='application/vnd.google-apps.folder' and trashed=false").arg(escapeQueryValue(folderName)));
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
        {
            m_folderId = files.first().toObject()["id"].toString();
            emit folderFound(m_folderId);
        }
    });
}

void DriveApi::listConversations(const QString &folderId)
{
    QUrlQuery query;
    // Revert the q parameter to fetch all active files in the folder
    query.addQueryItem("q", QString("'%1' in parents and trashed=false").arg(folderId));
    // ADDED: Request mimeType and fileExtension from Google Drive
    query.addQueryItem("fields", "files(id,name,modifiedTime,mimeType,fileExtension)");
    query.addQueryItem("orderBy", "modifiedTime desc");
    query.addQueryItem("pageSize", "1000");

    QUrl url("https://www.googleapis.com/drive/v3/files");
    url.setQuery(query);

    sendGet(url.toString(), [this](const QJsonObject &response)
            {
                QList<DriveFile> files;
                for (const auto &f : response["files"].toArray())
                {
                    auto obj = f.toObject();
                    QString id = obj["id"].toString();
                    QString name = obj["name"].toString();
                    QString mime = obj["mimeType"].toString();
                    QString ext = obj["fileExtension"].toString().toLower();

                    // 1. Filter out obvious media/document MIME types
                    if (mime.startsWith("image/") ||
                        mime.startsWith("video/") ||
                        mime.startsWith("audio/") ||
                        mime == "application/pdf" ||
                        mime == "text/csv")
                    {
                        continue;
                    }

                    // 2. Filter out files with common attachment extensions.
                    // Google AI Studio conversations are saved without a file extension.
                    // If it has an extension like .json or .txt, it's a user-uploaded attachment.
                    if (!ext.isEmpty())
                    {
                        QStringList attachmentExts = {
                            "json", "txt", "pdf", "csv", "md", "png", "jpg", "jpeg",
                            "mp3", "mp4", "wav", "py", "cpp", "h", "js", "html"
                        };
                        if (attachmentExts.contains(ext)) {
                            continue;
                        }
                    }

                    DriveFile df;
                    df.fileId = id;
                    df.fileName = name;
                    df.modifiedTime = QDateTime::fromString(obj["modifiedTime"].toString(), Qt::ISODate);
                    files.append(df);
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
            emit apiError("Download failed: " + reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        emit conversationDownloaded(fileId, fileName, doc.object());
    });
}

void DriveApi::downloadAttachment(const QString &fileId)
{
    Logger::instance().info(
        QString("[Attachment] downloadAttachment: fileId='%1'").arg(fileId));
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
            PendingRequest pending;
            pending.url = reply->url().toString();
            pending.binary = true;
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
                QString("[Attachment] downloadAttachment failed: fileId='%1' error='%2'")
                    .arg(fileId, reply->errorString()));
            emit apiError("Attachment download failed: " + reply->errorString());
            return;
        }
        QString mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        QByteArray data = reply->readAll();
        Logger::instance().info(
            QString("[Attachment] downloadAttachment OK: fileId='%1' mime='%2' size=%3")
                .arg(fileId, mime).arg(data.size()));
        emit attachmentDownloaded(fileId, data, mime);
    });
}

void DriveApi::searchConversations(const QString &query)
{
    if (m_folderId.isEmpty())
    {
        emit searchResultsReady({});
        return;
    }

    QUrlQuery q;
    q.addQueryItem("q", QString("'%1' in parents and fullText contains '%2' and trashed=false")
                       .arg(m_folderId, escapeQueryValue(query)));
    q.addQueryItem("fields", "files(id,name,modifiedTime,mimeType,fileExtension)");
    q.addQueryItem("pageSize", "100");

    QUrl url("https://www.googleapis.com/drive/v3/files");
    url.setQuery(q);

    sendGet(url.toString(), [this](const QJsonObject &response)
    {
        QList<DriveFile> results;
        for (const auto &f : response["files"].toArray())
        {
            auto obj = f.toObject();
            QString mime = obj["mimeType"].toString();
            QString ext = obj["fileExtension"].toString().toLower();

            if (mime.startsWith("image/") ||
                mime.startsWith("video/") ||
                mime.startsWith("audio/") ||
                mime == "application/pdf" ||
                mime == "text/csv")
            {
                continue;
            }

            if (!ext.isEmpty())
            {
                QStringList attachmentExts = {
                    "json", "txt", "pdf", "csv", "md", "png", "jpg", "jpeg",
                    "mp3", "mp4", "wav", "py", "cpp", "h", "js", "html",
                    "log", "cfg"
                };
                if (attachmentExts.contains(ext))
                    continue;
            }

            DriveFile df;
            df.fileId = obj["id"].toString();
            df.fileName = obj["name"].toString();
            df.modifiedTime = QDateTime::fromString(obj["modifiedTime"].toString(), Qt::ISODate);
            results.append(df);
        }
        emit searchResultsReady(results);
    });
}
