#include "ConversationModel.h"
#include "app/Logger.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

ConversationModel::ConversationModel(QObject *parent)
    : QObject(parent) {}

Conversation ConversationModel::parseConversation(const QString &fileId,
                                                   const QString &fileName,
                                                   const QJsonObject &root) const
{
    Conversation conv;
    conv.fileId = fileId;
    conv.fileName = fileName;

    // Run settings
    QJsonObject settings = root["runSettings"].toObject();
    conv.model = settings["model"].toString();
    conv.temperature = settings["temperature"].toDouble(1.0);
    conv.topP = settings["topP"].toDouble(0.95);
    conv.topK = settings["topK"].toInt(64);
    conv.maxOutputTokens = settings["maxOutputTokens"].toInt(8192);
    conv.systemInstruction = root["systemInstruction"].toObject();

    // Parse chunks
    QJsonArray chunks = root["chunkedPrompt"].toObject()["chunks"].toArray();
    for (const auto &chunk : chunks)
        conv.messages.append(Message::fromJson(chunk.toObject()));

    // Sort messages chronologically; user before model for same timestamp
    std::sort(conv.messages.begin(), conv.messages.end(),
        [](const Message &a, const Message &b)
        {
            if (a.createTime != b.createTime)
                return a.createTime < b.createTime;
            if (a.role != b.role)
                return a.role == "user";
            return false;
        });

    buildPairs(conv);

    // Log message order for attachment debugging
    for (int i = 0; i < conv.messages.size(); ++i)
    {
        const auto &m = conv.messages[i];
        if (!m.driveDocumentId.isEmpty() || m.role == "user")
            Logger::instance().info(
                QString("[Attachment] Msg[%1] role='%2' doc='%3' text='%4' time=%5")
                    .arg(i)
                    .arg(m.role)
                    .arg(m.driveDocumentId)
                    .arg(m.text.left(40))
                    .arg(m.createTime.toString(Qt::ISODate)));
    }

    return conv;
}

void ConversationModel::buildPairs(Conversation &conv) const
{
    Message currentPrompt;
    bool hasPrompt = false;

    for (const auto &msg : conv.messages)
    {
        if (msg.role == "user")
        {
            if (hasPrompt && msg.createTime == currentPrompt.createTime)
            {
                // Same timestamp — merge: combine driveDocumentId and text
                bool currentHasDoc = !currentPrompt.driveDocumentId.isEmpty();
                bool newHasDoc = !msg.driveDocumentId.isEmpty();
                bool currentHasText = !currentPrompt.text.isEmpty();
                bool newHasText = !msg.text.isEmpty();

                Logger::instance().info(
                    QString("[Attachment] Merge check at %1: "
                            "current(doc='%2' text=%3) new(doc='%4' text=%5)")
                        .arg(msg.createTime.toString(Qt::ISODate))
                        .arg(currentPrompt.driveDocumentId)
                        .arg(currentHasText)
                        .arg(msg.driveDocumentId)
                        .arg(newHasText));

                // Prefer the message that has text as the base
                if (newHasText && !currentHasText)
                {
                    QString docId = currentPrompt.driveDocumentId;
                    QString imgData = currentPrompt.inlineImageData;
                    QString imgMime = currentPrompt.inlineImageMime;
                    currentPrompt = msg;
                    if (currentPrompt.driveDocumentId.isEmpty())
                        currentPrompt.driveDocumentId = docId;
                    if (currentPrompt.inlineImageData.isEmpty())
                    {
                        currentPrompt.inlineImageData = imgData;
                        currentPrompt.inlineImageMime = imgMime;
                    }
                    Logger::instance().info(
                        QString("[Attachment] Merged doc-only into text prompt: docId='%1' text='%2'")
                            .arg(currentPrompt.driveDocumentId)
                            .arg(currentPrompt.text.left(60)));
                }
                else if (currentHasText && !newHasText)
                {
                    if (currentPrompt.driveDocumentId.isEmpty())
                        currentPrompt.driveDocumentId = msg.driveDocumentId;
                    if (currentPrompt.inlineImageData.isEmpty())
                    {
                        currentPrompt.inlineImageData = msg.inlineImageData;
                        currentPrompt.inlineImageMime = msg.inlineImageMime;
                    }
                    Logger::instance().info(
                        QString("[Attachment] Merged doc into text prompt: docId='%1' text='%2'")
                            .arg(currentPrompt.driveDocumentId)
                            .arg(currentPrompt.text.left(60)));
                }
                else if (!currentHasText && !newHasText)
                {
                    // Both doc-only — keep whichever has the driveDocumentId
                    if (currentPrompt.driveDocumentId.isEmpty())
                        currentPrompt.driveDocumentId = msg.driveDocumentId;
                }
                // else: both have text — keep current (first one wins)
            }
            else if (hasPrompt)
            {
                currentPrompt = msg;
            }
            else
            {
                currentPrompt = msg;
            }
            hasPrompt = true;
        }
        else if (msg.role == "model" && hasPrompt)
        {
            if (conv.pairs.isEmpty() ||
                conv.pairs.last().prompt.createTime != currentPrompt.createTime)
            {
                Conversation::PromptResponsePair pair;
                pair.prompt = currentPrompt;
                pair.responses.append(msg);
                conv.pairs.append(pair);
                if (!pair.prompt.driveDocumentId.isEmpty())
                    Logger::instance().info(
                        QString("[Attachment] Pair %1 created with docId='%2' text='%3'")
                            .arg(conv.pairs.size() - 1)
                            .arg(pair.prompt.driveDocumentId)
                            .arg(pair.prompt.text.left(60)));
            }
            else
            {
                conv.pairs.last().responses.append(msg);
            }
        }
    }

    // Flush last prompt if no model response followed
    if (hasPrompt)
    {
        bool found = false;
        for (const auto &p : conv.pairs)
        {
            if (p.prompt.createTime == currentPrompt.createTime)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            Conversation::PromptResponsePair pair;
            pair.prompt = currentPrompt;
            conv.pairs.append(pair);
            if (!pair.prompt.driveDocumentId.isEmpty())
                Logger::instance().info(
                    QString("[Attachment] Final pair %1 (no response) with docId='%2'")
                        .arg(conv.pairs.size() - 1)
                        .arg(pair.prompt.driveDocumentId));
        }
    }
}
