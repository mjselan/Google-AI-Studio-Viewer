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

    // Parse createTime — handle Z suffix robustly
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

    // Check parts for inline image data (pasted images)
    for (int i = 0; i < m.parts.size(); ++i)
    {
        QJsonObject part = m.parts[i].toObject();
        QStringList keys = part.keys();

        if (part.contains("inlineData"))
        {
            QJsonObject inlineData = part["inlineData"].toObject();
            m.inlineImageMime = inlineData["mimeType"].toString();
            m.inlineImageData = inlineData["data"].toString();
        }
    }

    return m;
}
