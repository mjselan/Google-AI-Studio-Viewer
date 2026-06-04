#include "Message.h"
#include "app/Logger.h"
#include <QJsonDocument>

Message Message::fromJson(const QJsonObject &obj)
{
    Message m;
    m.text = obj["text"].toString();
    m.role = obj["role"].toString();
    m.tokenCount = obj["tokenCount"].toInt();
    m.isThought = obj["isThought"].toBool(false);
    m.finishReason = obj["finishReason"].toString();
    m.parts = obj["parts"].toArray();

    // Parse createTime — handle Z suffix robustly
    QString ts = obj["createTime"].toString();
    m.createTime = QDateTime::fromString(ts, Qt::ISODateWithMs);
    if (!m.createTime.isValid() && ts.endsWith('Z'))
        m.createTime = QDateTime::fromString(ts.chopped(1) + "+00:00", Qt::ISODateWithMs);
    if (!m.createTime.isValid())
        Logger::instance().warn(
            QString("[Message] Failed to parse createTime: '%1'").arg(ts));

    if (obj.contains("driveDocument"))
    {
        QJsonObject dd = obj["driveDocument"].toObject();
        m.driveDocumentId = dd["id"].toString();
        Logger::instance().debug(
            QString("[Message] driveDocument found: id='%1' name='%2' mime='%3'")
                .arg(m.driveDocumentId, dd["name"].toString(), dd["mimeType"].toString()));
    }
    else if (obj.contains("driveImage"))
    {
        QJsonObject di = obj["driveImage"].toObject();
        m.driveDocumentId = di["id"].toString();
        Logger::instance().info(
            QString("[Message] driveImage found: id='%1'").arg(m.driveDocumentId));
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

    // Log the full chunk if empty-text with no drive reference — likely image-only
    if (m.text.isEmpty() && m.driveDocumentId.isEmpty() && m.role == "user")
    {
        Logger::instance().warn(
            QString("[Message] Empty-text user chunk with %1 parts. Full object: %2")
                .arg(m.parts.size())
                .arg(QString(QJsonDocument(obj).toJson(QJsonDocument::Compact))));
    }

    return m;
}
