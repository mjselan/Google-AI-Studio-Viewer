#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QList>

struct InlineAttachment
{
    QString mimeType;
    QString data;
};

struct Message
{
    QString text;
    QString role;
    int tokenCount = 0;
    QDateTime createTime;
    bool isThought = false;
    QString finishReason;
    QString driveDocumentId;

    // Supports multiple inline images per message
    QList<InlineAttachment> inlineImages;

    // Legacy single-attachment fields retained for compatibility
    QString inlineImageData;
    QString inlineImageMime;
    QJsonArray parts;

    static Message fromJson(const QJsonObject &obj);
};