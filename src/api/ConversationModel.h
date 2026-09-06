#pragma once

#include <QObject>
#include <QList>
#include <QJsonObject>
#include <QDateTime>
#include "models/Conversation.h"

class LocalCache;

class ConversationModel : public QObject
{
    Q_OBJECT
public:
    explicit ConversationModel(QObject *parent = nullptr);

    Conversation parseConversation(const QString &fileId, const QString &fileName,
                                   const QJsonObject &root) const;
    static bool isLikelyConversation(const QJsonObject &root);

    // Unified ingest pipeline: validate -> cache -> parse (Item #7)
    bool ingestConversation(const QString &fileId,
                            const QString &fileName,
                            const QDateTime &modTime,
                            const QJsonObject &data,
                            LocalCache *cache,
                            Conversation &outConv) const;

private:
    void buildPairs(Conversation &conv) const;
};