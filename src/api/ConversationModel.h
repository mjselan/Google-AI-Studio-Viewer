#pragma once

#include <QObject>
#include <QList>
#include <QJsonObject>
#include "models/Conversation.h"

class ConversationModel : public QObject
{
    Q_OBJECT
public:
    explicit ConversationModel(QObject *parent = nullptr);

    Conversation parseConversation(const QString &fileId, const QString &fileName,
                                   const QJsonObject &root) const;
    static bool isLikelyConversation(const QJsonObject &root);

private:
    void buildPairs(Conversation &conv) const;
};
