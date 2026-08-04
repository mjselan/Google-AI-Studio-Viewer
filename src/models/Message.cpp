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

    // Debug: confirm whether any attachment was parsed from the raw JSON chunk
    bool hadDriveKey = obj.contains("driveDocument") || obj.contains("driveImage");
    if (!m.driveDocumentId.isEmpty() || !m.inlineImageData.isEmpty() || hadDriveKey)
    {
        Logger::instance().debug(
            QString("[Attachment] Msg::fromJson role='%1' rawHadDriveKey=%2 driveDocId='%3' "
                    "imageMime='%4' imageLen=%5 text='%6' time='%7'")
                .arg(m.role)
                .arg(hadDriveKey ? "true" : "false")
                .arg(m.driveDocumentId,
                     m.inlineImageMime)
                .arg(m.inlineImageData.size())
                .arg(m.text.left(40))
                .arg(m.createTime.toString(Qt::ISODate)));
    }

    return m;
}
