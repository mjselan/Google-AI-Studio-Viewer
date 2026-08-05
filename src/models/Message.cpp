#include "Message.h"
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

    QString ts = obj["createTime"].toString();
    m.createTime = QDateTime::fromString(ts, Qt::ISODateWithMs);
    if (!m.createTime.isValid() && ts.endsWith('Z'))
        m.createTime = QDateTime::fromString(ts.chopped(1) + "+00:00", Qt::ISODateWithMs);

    if (obj.contains("driveDocument"))
    {
        QJsonObject dd = obj["driveDocument"].toObject();
        m.driveDocumentId = dd["id"].toString();
    }
    else if (obj.contains("driveImage"))
    {
        QJsonObject di = obj["driveImage"].toObject();
        m.driveDocumentId = di["id"].toString();
    }

    // Process parts array for attachments and text fallbacks
    QStringList textParts;
    for (int i = 0; i < m.parts.size(); ++i)
    {
        QJsonObject part = m.parts[i].toObject();

        if (part.contains("inlineData"))
        {
            QJsonObject inlineData = part["inlineData"].toObject();
            InlineAttachment img;
            img.mimeType = inlineData["mimeType"].toString();
            img.data = inlineData["data"].toString();
            m.inlineImages.append(img);
        }

        if (part.contains("text"))
        {
            textParts.append(part["text"].toString());
        }
    }

    // Fallback: If root level text is empty, reconstruct from parts
    if (m.text.isEmpty() && !textParts.isEmpty())
    {
        m.text = textParts.join("\n");
    }

    // Backwards compatibility for single inline image references
    if (!m.inlineImages.isEmpty())
    {
        m.inlineImageData = m.inlineImages.first().data;
        m.inlineImageMime = m.inlineImages.first().mimeType;
    }

    return m;
}