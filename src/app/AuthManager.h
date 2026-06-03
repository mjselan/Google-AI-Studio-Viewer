#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QNetworkAccessManager>

class AuthManager : public QObject
{
    Q_OBJECT
public:
    explicit AuthManager(QObject *parent = nullptr);

    bool isAuthenticated() const;
    QString accessToken() const;

    void loadConfig(const QString &configPath);
    void authenticate();
    void refreshAccessToken();
    void signOut();
    void submitAuthCode(const QString &code);

signals:
    void authSuccess();
    void authFailed(const QString &error);
    void tokenRefreshed();
    void authCodeRequired(const QString &authUrl);

private:
    void exchangeCodeForTokens(const QString &code);
    void storeTokens(const QString &access, const QString &refresh, int expiresIn);
    QString buildAuthUrl() const;

    QNetworkAccessManager *m_net;

    QString m_clientId;
    QString m_clientSecret;
    QString m_redirectUri;
    QStringList m_scopes;

    QString m_accessToken;
    QString m_refreshToken;
    int m_expiresIn = 0;
};
