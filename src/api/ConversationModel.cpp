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
    Logger::instance().debug(
        QString("[ConversationModel] '%1': %2 chunks, %3 pairs (before build)")
            .arg(fileName).arg(chunks.size()).arg(conv.pairs.size()));
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
                Logger::instance().debug(
                    QString("[buildPairs] Consecutive user message at %1, "
                            "previous prompt at %2 has no response yet — replacing")
                        .arg(msg.createTime.toString(Qt::ISODate))
                        .arg(currentPrompt.createTime.toString(Qt::ISODate)));
            }
            currentPrompt = msg;
            hasPrompt = true;
        }
        else if (msg.role == "model" && hasPrompt)
        {
            if (conv.pairs.isEmpty() ||
                conv.pairs.last().prompt.createTime != currentPrompt.createTime)
            {
                Logger::instance().debug(
                    QString("[buildPairs] New pair: prompt at %1, model at %2 text='%3'")
                        .arg(currentPrompt.createTime.toString(Qt::ISODate))
                        .arg(msg.createTime.toString(Qt::ISODate))
                        .arg(msg.text.left(60)));
                Conversation::PromptResponsePair pair;
                pair.prompt = currentPrompt;
                pair.responses.append(msg);
                conv.pairs.append(pair);
            }
            else
            {
                Logger::instance().debug(
                    QString("[buildPairs] Appending response to pair %1 (model at %2)")
                        .arg(conv.pairs.size() - 1)
                        .arg(msg.createTime.toString(Qt::ISODate)));
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
            Logger::instance().debug(
                QString("[buildPairs] Final prompt at %1 has no response — adding empty pair")
                    .arg(currentPrompt.createTime.toString(Qt::ISODate)));
            Conversation::PromptResponsePair pair;
            pair.prompt = currentPrompt;
            conv.pairs.append(pair);
        }
    }

    Logger::instance().info(
        QString("[buildPairs] '%1': %2 pairs built from %3 messages")
            .arg(conv.fileName).arg(conv.pairs.size()).arg(conv.messages.size()));
}
