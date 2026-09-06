#include "ConversationSyncManager.h"
#include "app/Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

ConversationSyncManager::ConversationSyncManager(LocalCache *cache, ConversationModel *model, QObject *parent)
    : QObject(parent)
    , m_cache(cache)
    , m_model(model)
    , m_net(new QNetworkAccessManager(this))
    , m_pacingTimer(new QTimer(this))
{
    m_pacingTimer->setInterval(REQUEST_PACING_MS);
    connect(m_pacingTimer, &QTimer::timeout, this, &ConversationSyncManager::processNextQueueItem);
}

ConversationSyncManager::~ConversationSyncManager()
{
    cancelSync();
}

void ConversationSyncManager::setAccessToken(const QString &token)
{
    m_token = token;
    if (m_pausedForToken && m_syncing)
    {
        m_pausedForToken = false;
        m_pacingTimer->start();
    }
}

void ConversationSyncManager::cancelSync()
{
    m_pacingTimer->stop();
    m_queue.clear();
    m_activeRequests = 0;
    m_syncing = false;
    m_pausedForToken = false;
}

void ConversationSyncManager::startSync(const QList<DriveFile> &driveFiles)
{
    cancelSync();
    m_syncing = true;

    QSet<QString> remoteFileIds;
    QList<DriveFile> filesToFetch;
    int upToDateCount = 0;

    for (const auto &df : driveFiles)
    {
        remoteFileIds.insert(df.fileId);

        // Check if cached file exists and has matching modifiedTime
        QDateTime cachedTime = m_cache->getModifiedTime(df.fileId);
        if (cachedTime.isValid() && cachedTime.toSecsSinceEpoch() == df.modifiedTime.toSecsSinceEpoch())
        {
            upToDateCount++;
        }
        else
        {
            filesToFetch.append(df);
        }
    }

    // Prune local cache entries for files deleted/trashed in Google Drive
    QSet<QString> cachedIds = m_cache->getAllCachedFileIds();
    for (const QString &cachedId : cachedIds)
    {
        if (!remoteFileIds.contains(cachedId))
        {
            Logger::instance().info("Pruning deleted conversation from cache: " + cachedId);
            m_cache->deleteConversation(cachedId);
        }
    }

    if (filesToFetch.isEmpty())
    {
        m_syncing = false;
        emit syncFinished(0, upToDateCount);
        return;
    }

    for (const auto &df : filesToFetch)
    {
        m_queue.enqueue(DownloadTask{df, 0});
    }

    m_totalTasks = m_queue.size();
    m_completedTasks = 0;

    emit syncStarted(m_totalTasks);
    m_pacingTimer->start();
}

void ConversationSyncManager::processNextQueueItem()
{
    if (m_pausedForToken || !m_syncing)
        return;

    if (m_queue.isEmpty())
    {
        if (m_activeRequests == 0)
        {
            m_pacingTimer->stop();
            m_syncing = false;
            emit syncFinished(m_completedTasks, m_totalTasks - m_completedTasks);
        }
        return;
    }

    if (m_activeRequests >= MAX_CONCURRENT_REQUESTS)
        return;

    DownloadTask task = m_queue.dequeue();
    m_activeRequests++;
    startDownload(task);
}

void ConversationSyncManager::startDownload(const DownloadTask &task)
{
    QUrl url(QString("https://www.googleapis.com/drive/v3/files/%1?alt=media").arg(task.file.fileId));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, task]()
            {
                handleReply(reply, task);
            });
}

void ConversationSyncManager::handleReply(QNetworkReply *reply, DownloadTask task)
{
    reply->deleteLater();
    m_activeRequests = qMax(0, m_activeRequests - 1);

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 1. Handle Token Expiration (401)
    if (statusCode == 401)
    {
        m_pausedForToken = true;
        m_queue.prepend(task); // Put back to front of queue
        emit tokenNeedsRefresh();
        return;
    }

    // 2. Handle Rate-Limiting (429) or Google Backend Temporary Error (500/503)
    if (statusCode == 429 || statusCode == 500 || statusCode == 503)
    {
        task.retryCount++;
        if (task.retryCount <= 5)
        {
            // Exponential Backoff with Jitter: (2^retry * 1000ms) + random(100..500ms)
            int backoffMs = (1 << task.retryCount) * 1000 + QRandomGenerator::global()->bounded(100, 500);
            Logger::instance().warn(QString("Drive API rate-limited (HTTP %1). Backing off for %2 ms...")
                                        .arg(statusCode).arg(backoffMs));

            m_pacingTimer->stop();
            QTimer::singleShot(backoffMs, this, [this, task]()
                               {
                                   m_queue.prepend(task);
                                   if (m_syncing && !m_pausedForToken)
                                       m_pacingTimer->start();
                               });
            return;
        }
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        Logger::instance().error(QString("Failed to sync file '%1': %2")
                                     .arg(task.file.fileName, reply->errorString()));
        m_completedTasks++;
        emit syncProgress(m_completedTasks, m_totalTasks, task.file.fileName);
        return;
    }

    // Parse and save to SQLite
    auto doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject data = doc.object();

    Conversation conv;
    if (m_model->ingestConversation(task.file.fileId, task.file.fileName, task.file.modifiedTime, data, m_cache, conv))
    {
        emit conversationSynced(conv);
    }
    else
    {
        emit conversationInvalid(task.file.fileId);
    }

    m_completedTasks++;
    emit syncProgress(m_completedTasks, m_totalTasks, task.file.fileName);
}