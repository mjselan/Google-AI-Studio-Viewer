#include "ConversationModel.h"
#include "app/Logger.h"
#include <QJsonArray>
#include <QJsonDocument>

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
    Logger::instance().debug(
        QString("[ConversationModel] '%1': %2 chunks, %3 pairs (before build)")
            .arg(fileName).arg(chunks.size()).arg(conv.pairs.size()));
    for (const auto &chunk : chunks)
        conv.messages.append(Message::fromJson(chunk.toObject()));

    Logger::instance().info(
        QString("[ConversationModel] '%1' parsed: %2 messages")
            .arg(fileName).arg(conv.messages.size()));

    buildPairs(conv);

    // Log pairs with driveDocumentIds
    for (int i = 0; i < conv.pairs.size(); ++i)
    {
        const auto &p = conv.pairs[i];
        if (!p.prompt.driveDocumentId.isEmpty())
            Logger::instance().info(
                QString("[ConversationModel] Pair %1 has driveDocumentId='%2'")
                    .arg(i).arg(p.prompt.driveDocumentId));
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
            if (hasPrompt)
            {
                Conversation::PromptResponsePair pair;
                pair.prompt = currentPrompt;
                conv.pairs.append(pair);
            }
            currentPrompt = msg;
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
