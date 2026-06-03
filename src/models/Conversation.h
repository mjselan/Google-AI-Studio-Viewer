#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QDateTime>
#include "Message.h"

struct Conversation
{
    QString fileId;
    QString fileName;
    QDateTime lastModified;

    // Run settings
    QString model;
    double temperature = 1.0;
    double topP = 0.95;
    int topK = 64;
    int maxOutputTokens = 8192;
    QJsonObject systemInstruction;

    // All chunks parsed as flat messages
    QList<Message> messages;

    // Paired: each user message → subsequent model response(s)
    struct PromptResponsePair
    {
        Message prompt;
        QList<Message> responses;
    };
    QList<PromptResponsePair> pairs;

    int totalTokenCount() const;
    QString displayTitle() const;
    QDateTime lastMessageTime() const;
};
