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

static QString escapeQueryValue(const QString &value)
{
    QString escaped = value;
    escaped.replace('\\', "\\\\");
    escaped.replace('\'', "\\'");
    return escaped;
}

static bool isIgnoredAttachment(const QString &mime, const QString &ext)
{
    if (mime.startsWith("image/") || mime.startsWith("video/") ||
        mime.startsWith("audio/") || mime == "application/pdf" || mime == "text/csv")
    {
        return true;
    }

    if (!ext.isEmpty())
    {
        static const QStringList attachmentExts = {
            "json", "txt", "pdf", "csv", "md", "png", "jpg", "jpeg",
            "mp3", "mp4", "wav", "py", "cpp", "h", "js", "html", "log", "cfg"
        };
        if (attachmentExts.contains(ext))
            return true;
    }
    return false;
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
                    // 🔥 FIX: Guard against multiple simultaneous refresh requests
                    if (!m_refreshInProgress)
                    {
                        m_refreshInProgress = true;
                        emit tokenNeedsRefresh();
                    }
                    return;
                }

                if (reply->error() != QNetworkReply::NoError)
                {
                    QString errStr = QString("Drive API HTTP %1 error: %2").arg(status).arg(reply->errorString());
                    qWarning() << errStr;
                    emit apiError(errStr);
                    return;
                }

                auto doc = QJsonDocument::fromJson(reply->readAll());
                callback(doc.object());
            });
}

void DriveApi::retryPendingRequests()
{
    m_refreshInProgress = false; // 🔥 Reset guard when tokens are refreshed
    QList<PendingRequest> retries = m_pendingRequests;
    m_pendingRequests.clear();
    for (const auto &pending : retries)
    {
        if (pending.binary)
            pending.callback(QJsonObject());
        else
            sendGet(pending.url, pending.callback);
    }
}

void DriveApi::findFolder(const QString &folderName)
{
    QUrlQuery query;
    query.addQueryItem("q", QString("name='%1' and mimeType='application/vnd.google-apps.folder' and trashed=false")
                                .arg(escapeQueryValue(folderName)));
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
    query.addQueryItem("q", QString("'%1' in parents and trashed=false").arg(escapeQueryValue(folderId)));
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
                    if (isIgnoredAttachment(obj["mimeType"].toString(), obj["fileExtension"].toString().toLower()))
                        continue;

                    DriveFile df;
                    df.fileId = obj["id"].toString();
                    df.fileName = obj["name"].toString();
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
                    pending.callback = [this, fileId](const QJsonObject &) { downloadAttachment(fileId); };
                    m_pendingRequests.append(pending);
                    emit tokenNeedsRefresh();
                    return;
                }

                if (reply->error() != QNetworkReply::NoError)
                {
                    emit apiError("Attachment download failed: " + reply->errorString());
                    return;
                }
                QString mime = reply->header(QNetworkRequest::ContentTypeHeader).toString();
                emit attachmentDownloaded(fileId, reply->readAll(), mime);
            });
}

void DriveApi::searchConversations(const QString &query, bool includeAttachments)
{
    if (m_folderId.isEmpty())
    {
        emit searchResultsReady({});
        return;
    }

    QString escapedFolder = escapeQueryValue(m_folderId);
    QString escapedQuery = escapeQueryValue(query);

    QUrlQuery q;
    q.addQueryItem("q", QString("'%1' in parents and (name contains '%2' or fullText contains '%2') and trashed=false")
                            .arg(escapedFolder, escapedQuery));
    q.addQueryItem("fields", "files(id,name,modifiedTime,mimeType,fileExtension)");
    q.addQueryItem("pageSize", "100");

    QUrl url("https://www.googleapis.com/drive/v3/files");
    url.setQuery(q);

    sendGet(url.toString(), [this, includeAttachments](const QJsonObject &response)
            {
                QList<DriveFile> results;
                for (const auto &f : response["files"].toArray())
                {
                    auto obj = f.toObject();
                    QString mime = obj["mimeType"].toString();
                    QString ext = obj["fileExtension"].toString().toLower();

                    // Skip attachments ONLY if the user has not enabled "Search all attachments"
                    if (!includeAttachments && isIgnoredAttachment(mime, ext))
                        continue;

                    DriveFile df;
                    df.fileId = obj["id"].toString();
                    df.fileName = obj["name"].toString();
                    df.modifiedTime = QDateTime::fromString(obj["modifiedTime"].toString(), Qt::ISODate);
                    results.append(df);
                }
                emit searchResultsReady(results);
            });
}
QString DriveFile::info()
{
    return "DriveFile info: " + fileId + ", " + fileName + ", " + modifiedTime.toUTC().toString(Qt::ISODate);
}
