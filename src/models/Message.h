#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>

struct Message
{
    QString text;
    QString role;               // "user" or "model"
    int tokenCount = 0;
    QDateTime createTime;
    bool isThought = false;
    QString finishReason;
    QString driveDocumentId;     // non-empty if this chunk is a file upload
    QString inlineImageData;     // base64-encoded inline image data
    QString inlineImageMime;     // mime type of inline image
    QJsonArray parts;           // raw parts array for thought signatures etc.

    static Message fromJson(const QJsonObject &obj);
};
