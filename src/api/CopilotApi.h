#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

class CopilotApi : public QObject
{
    Q_OBJECT
public:
    explicit CopilotApi(QObject *parent = nullptr);

    void setBaseUrl(const QString &url);
    void fetchModels();
    void sendMessage(const QString &prompt, const QString &conversationId = QString());

signals:
    void connected(const QString &modelName);
    void connectionFailed(const QString &error);
    void responseReceived(const QString &content, const QString &conversationId);
    void errorOccurred(const QString &error);

private:
    QNetworkAccessManager *m_net;
    QString m_baseUrl;
    QString m_modelName;
};
