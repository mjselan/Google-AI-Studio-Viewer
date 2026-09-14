#include "ConversationModel.h"
#include "LocalCache.h"
#include "app/Logger.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

ConversationModel::ConversationModel(QObject *parent)
    : QObject(parent) {}

bool ConversationModel::ingestConversation(const QString &fileId,
                                           const QString &fileName,
                                           const QDateTime &modTime,
                                           const QJsonObject &data,
                                           LocalCache *cache,
                                           Conversation &outConv) const
{
    if (!isLikelyConversation(data))
    {
        return false;
    }

    if (cache)
    {
        cache->saveConversation(fileId, fileName, modTime, data);
    }

    outConv = parseConversation(fileId, fileName, data);
    return true;
}

Conversation ConversationModel::parseConversation(const QString &fileId,
                                                  const QString &fileName,
                                                  const QJsonObject &root) const
{
    Conversation conv;
    conv.fileId = fileId;
    conv.fileName = fileName;

    QJsonObject settings = root["runSettings"].toObject();
    conv.model = settings["model"].toString();
    conv.temperature = settings["temperature"].toDouble(1.0);
    conv.topP = settings["topP"].toDouble(0.95);
    conv.topK = settings["topK"].toInt(64);
    conv.maxOutputTokens = settings["maxOutputTokens"].toInt(8192);
    conv.systemInstruction = root["systemInstruction"].toObject();

    QJsonArray chunks = root["chunkedPrompt"].toObject()["chunks"].toArray();
    for (const auto &chunk : chunks)
        conv.messages.append(Message::fromJson(chunk.toObject()));

    // Item #3: Use std::stable_sort to preserve relative ordering for chunks with identical timestamps
    std::stable_sort(conv.messages.begin(), conv.messages.end(),
              [](const Message &a, const Message &b)
              {
                  qint64 ta = a.createTime.isValid() ? a.createTime.toMSecsSinceEpoch() : 0;
                  qint64 tb = b.createTime.isValid() ? b.createTime.toMSecsSinceEpoch() : 0;
                  if (ta != tb)
                      return ta < tb;
                  if (a.role != b.role)
                      return a.role == "user";
                  return false;
              });

    buildPairs(conv);
    return conv;
}

bool ConversationModel::isLikelyConversation(const QJsonObject &root)
{
    return root.contains("runSettings") && root["runSettings"].isObject();
}

void ConversationModel::buildPairs(Conversation &conv) const
{
    conv.pairs.clear();

    Message currentPrompt;
    bool promptActive = false;
    bool modelResponded = false;

    for (const auto &msg : conv.messages)
    {
        if (msg.role == "user")
        {
            if (modelResponded)
            {
                // Previous turn had a model response -> this user message starts a brand new turn
                currentPrompt = msg;
                promptActive = true;
                modelResponded = false;
            }
            else if (promptActive)
            {
                // Consecutive user chunks before model responds
                qint64 tCurrent = currentPrompt.createTime.isValid() ? currentPrompt.createTime.toMSecsSinceEpoch() : 0;
                qint64 tMsg = msg.createTime.isValid() ? msg.createTime.toMSecsSinceEpoch() : 0;

                if (tMsg == tCurrent || msg.text.isEmpty() || currentPrompt.text.isEmpty())
                {
                    // Same timestamp or complementary chunks (text + file upload) -> merge
                    if (currentPrompt.text.isEmpty() && !msg.text.isEmpty())
                        currentPrompt.text = msg.text;
                    if (currentPrompt.driveDocumentId.isEmpty() && !msg.driveDocumentId.isEmpty())
                        currentPrompt.driveDocumentId = msg.driveDocumentId;
                    if (currentPrompt.inlineImageData.isEmpty() && !msg.inlineImageData.isEmpty())
                    {
                        currentPrompt.inlineImageData = msg.inlineImageData;
                        currentPrompt.inlineImageMime = msg.inlineImageMime;
                        currentPrompt.inlineImages = msg.inlineImages;
                    }
                }
                else
                {
                    // User sent a new prompt before model replied -> flush previous prompt (0 responses)
                    Conversation::PromptResponsePair pair;
                    pair.prompt = currentPrompt;
                    conv.pairs.append(pair);

                    currentPrompt = msg;
                }
            }
            else
            {
                currentPrompt = msg;
                promptActive = true;
                modelResponded = false;
            }
        }
        else if (msg.role == "model")
        {
            if (promptActive)
            {
                if (!modelResponded)
                {
                    Conversation::PromptResponsePair pair;
                    pair.prompt = currentPrompt;
                    pair.responses.append(msg);
                    conv.pairs.append(pair);
                    modelResponded = true;
                }
                else
                {
                    if (!conv.pairs.isEmpty())
                        conv.pairs.last().responses.append(msg);
                }
            }
            else
            {
                // Item #5: Handle model message received before any user prompt without dropping data
                //Logger::instance().warn("buildPairs: Model response found before any user prompt: " + msg.text.left(60));
                Conversation::PromptResponsePair pair;
                pair.prompt.text = "[Initial Model Output]";
                pair.prompt.role = "user";
                pair.prompt.createTime = msg.createTime;
                pair.responses.append(msg);
                conv.pairs.append(pair);
            }
        }
    }

    // Flush trailing user prompt if no model response followed
    if (promptActive && !modelResponded)
    {
        Conversation::PromptResponsePair pair;
        pair.prompt = currentPrompt;
        conv.pairs.append(pair);
    }
}