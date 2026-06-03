#include "Message.h"
#include "app/Logger.h"
#include <QJsonDocument>

Message Message::fromJson(const QJsonObject &obj)
{
    Message m;
    m.text = obj["text"].toString();
    m.role = obj["role"].toString();
    m.tokenCount = obj["tokenCount"].toInt();
    m.createTime = QDateTime::fromString(obj["createTime"].toString(), Qt::ISODateWithMs);
    m.isThought = obj["isThought"].toBool(false);
    m.finishReason = obj["finishReason"].toString();
    m.parts = obj["parts"].toArray();

    if (obj.contains("driveDocument"))
    {
        QJsonObject dd = obj["driveDocument"].toObject();
        m.driveDocumentId = dd["id"].toString();
        Logger::instance().debug(
            QString("[Message] driveDocument found: id='%1' name='%2' mime='%3'")
                .arg(m.driveDocumentId, dd["name"].toString(), dd["mimeType"].toString()));
    }

    // Check parts for inline image data (pasted images)
    for (int i = 0; i < m.parts.size(); ++i)
    {
        QJsonObject part = m.parts[i].toObject();
        QStringList keys = part.keys();
        Logger::instance().debug(
            QString("[Message] part[%1] role='%2' keys=%3 text.len=%4")
                .arg(i).arg(m.role).arg(keys.join(","))
                .arg(part["text"].toString().length()));

        if (part.contains("inlineData"))
        {
            QJsonObject inlineData = part["inlineData"].toObject();
            m.inlineImageMime = inlineData["mimeType"].toString();
            m.inlineImageData = inlineData["data"].toString();
            Logger::instance().info(
                QString("[Message] Found inline image: mime=%1 data.len=%2")
                    .arg(m.inlineImageMime).arg(m.inlineImageData.length()));
        }
    }

    // Log the full chunk if no text and no driveDocument — likely image-only
    if (m.text.isEmpty() && m.driveDocumentId.isEmpty() && m.role == "user")
    {
        Logger::instance().warn(
            QString("[Message] Empty-text user chunk with %1 parts. Full object: %2")
                .arg(m.parts.size())
                .arg(QString(QJsonDocument(obj).toJson(QJsonDocument::Compact))));
    }

    return m;
}
