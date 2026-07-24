#include "CopilotApi.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>

CopilotApi::CopilotApi(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_baseUrl("http://localhost:8000")
{
}

void CopilotApi::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
}

void CopilotApi::fetchModels()
{
    QUrl url(m_baseUrl + "/v1/models");
    QNetworkRequest req(url);

    auto *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        reply->deleteLater();

        auto doc = QJsonDocument::fromJson(reply->readAll());

        if (reply->error() != QNetworkReply::NoError)
        {
            QString errMsg = reply->errorString();
            emit connectionFailed(errMsg);
            return;
        }
        auto data = doc.object()["data"].toArray();
        if (!data.isEmpty())
        {
            m_modelName = data.first().toObject()["id"].toString();
            emit connected(m_modelName);
        }
        else
        {
            emit connectionFailed("No models returned");
        }
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

        if (reply->error() != QNetworkReply::NoError)
        {
            QString errMsg;
            if (root.contains("error"))
                errMsg = root["error"].toObject()["message"].toString();
            else
                errMsg = reply->errorString();

            emit errorOccurred(errMsg);
            return;
        }

        if (root.contains("error"))
        {
            QString errMsg = root["error"].toObject()["message"].toString();
            emit errorOccurred(errMsg);
            return;
        }

        QString content = root["choices"].toArray()
            .first().toObject()["message"].toObject()["content"].toString();
        QString convId = root["conversation_id"].toString();

        emit responseReceived(content, convId);
    });
}
