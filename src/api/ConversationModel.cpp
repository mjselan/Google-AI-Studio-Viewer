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

    // Sort chronologically using epoch milliseconds
    std::sort(conv.messages.begin(), conv.messages.end(),
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
    // Google AI Studio chat logs ALWAYS have a "runSettings" object
    // and usually a "chunkedPrompt" or "systemInstruction"
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
        else if (msg.role == "model" && promptActive)
        {
            if (!modelResponded)
            {
                // First model response for current turn -> create new pair
                Conversation::PromptResponsePair pair;
                pair.prompt = currentPrompt;
                pair.responses.append(msg);
                conv.pairs.append(pair);
                modelResponded = true;
            }
            else
            {
                // Subsequent model chunk for current turn -> append to existing pair
                if (!conv.pairs.isEmpty())
                    conv.pairs.last().responses.append(msg);
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