#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QTimer>
#include <QDateTime>
#include <QSet>
#include <QString>
#include <QList>

#include "models/Conversation.h"
#include "api/DriveApi.h"
#include "api/LocalCache.h"
#include "api/ConversationModel.h"

class ConversationSyncManager : public QObject
{
    Q_OBJECT
public:
    explicit ConversationSyncManager(LocalCache *cache, ConversationModel *model, QObject *parent = nullptr);
    ~ConversationSyncManager() override;

    void setAccessToken(const QString &token);
    void startSync(const QList<DriveFile> &driveFiles);
    void cancelSync();
    bool isSyncing() const { return m_syncing; }

signals:
    void syncStarted(int totalToDownload);
    void syncProgress(int completed, int total, const QString &currentFileName);
    void conversationSynced(const Conversation &conv);
    void syncFinished(int downloadedCount, int upToDateCount);
    void tokenNeedsRefresh();
    void syncError(const QString &error);

private slots:
    void processNextQueueItem();

private:
    struct DownloadTask
    {
        DriveFile file;
        int retryCount = 0;
    };

    void startDownload(const DownloadTask &task);
    void handleReply(QNetworkReply *reply, DownloadTask task);

    LocalCache *m_cache;
    ConversationModel *m_model;
    QNetworkAccessManager *m_net;
    QString m_token;

    QQueue<DownloadTask> m_queue;
    int m_totalTasks = 0;
    int m_completedTasks = 0;
    int m_activeRequests = 0;
    bool m_syncing = false;
    bool m_pausedForToken = false;

    const int MAX_CONCURRENT_REQUESTS = 2;
    const int REQUEST_PACING_MS = 60;
    QTimer *m_pacingTimer;
};