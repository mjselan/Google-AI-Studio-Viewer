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

    // Debug: record which pairs carry attachments and whether any attachment
    // message got dropped during pairing (never became a pair prompt).
    for (int i = 0; i < conv.pairs.size(); ++i)
    {
        const auto &p = conv.pairs[i].prompt;
        Logger::instance().debug(
            QString("[Attachment] Pair %1/%2 prompt: docId='%3' hasText=%4 imageLen=%5 createTime='%6'")
                .arg(i)
                .arg(conv.pairs.size())
                .arg(p.driveDocumentId)
                .arg(p.text.isEmpty() ? "false" : "true")
                .arg(p.inlineImageData.size())
                .arg(p.createTime.toString(Qt::ISODate)));
    }

    for (const auto &m : conv.messages)
    {
        if (m.driveDocumentId.isEmpty() && m.inlineImageData.isEmpty())
            continue;

        bool inPairPrompt = false;
        for (const auto &p : conv.pairs)
        {
            if (p.prompt.driveDocumentId == m.driveDocumentId &&
                p.prompt.createTime == m.createTime)
            {
                inPairPrompt = true;
                break;
            }
        }
        if (!inPairPrompt)
        {
            Logger::instance().warn(
                QString("[Attachment] LOST during pairing: role='%1' docId='%2' imageLen=%3 "
                        "text='%4' createTime='%5' did not end up as any pair prompt")
                    .arg(m.role, m.driveDocumentId)
                    .arg(m.inlineImageData.size())
                    .arg(m.text.left(40))
                    .arg(m.createTime.toString(Qt::ISODate)));
        }
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
        }
    }
}
