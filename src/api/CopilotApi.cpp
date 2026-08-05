#include "CopilotApi.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>

CopilotApi::CopilotApi(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_baseUrl("http://localhost:8000")
{
}

void CopilotApi::setBaseUrl(const QString &url) { m_baseUrl = url; }

void CopilotApi::fetchModels()
{
    QUrl url(m_baseUrl + "/v1/models");
    QNetworkRequest req(url);

    auto *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError)
                {
                    emit connectionFailed(reply->errorString());
                    return;
                }

                auto doc = QJsonDocument::fromJson(reply->readAll());
                auto data = doc.object()["data"].toArray();
                if (data.isEmpty())
                {
                    emit connectionFailed("No models returned by server.");
                    return;
                }

                m_modelName = data.first().toObject()["id"].toString();
                emit connected(m_modelName);
            });
}

void CopilotApi::sendMessage(const QString &prompt, const QString &conversationId)
{
    QJsonObject body;
    body["model"] = m_modelName.isEmpty() ? "copilot" : m_modelName;
    body["stream"] = false;

    QJsonArray messages;
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = prompt;
    messages.append(msg);
    body["messages"] = messages;

    if (!conversationId.isEmpty())
        body["conversation_id"] = conversationId;

    QUrl url(m_baseUrl + "/v1/chat/completions");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *reply = m_net->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();
                auto doc = QJsonDocument::fromJson(reply->readAll());
                auto root = doc.object();

                if (reply->error() != QNetworkReply::NoError || root.contains("error"))
                {
                    QString errMsg = root.contains("error")
                    ? root["error"].toObject()["message"].toString()
                    : reply->errorString();
                    emit errorOccurred(errMsg);
                    return;
                }

                QJsonArray choices = root["choices"].toArray();
                if (choices.isEmpty())
                {
                    emit errorOccurred("Server returned empty choices array.");
                    return;
                }

                QJsonObject firstChoice = choices.first().toObject();
                QString content = firstChoice["message"].toObject()["content"].toString();
                QString convId = root["conversation_id"].toString();

                emit responseReceived(content, convId);
            });
}