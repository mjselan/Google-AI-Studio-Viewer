#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QDateTime>
#include <QHash>


class QTcpServer;
class QTcpSocket;

class AuthManager : public QObject
{
    Q_OBJECT
public:
    explicit AuthManager(QObject *parent = nullptr);

    bool isAuthenticated() const;
    QString accessToken() const;
    int expiresIn() const;

    void loadConfig(const QString &configPath);
    void setCredentials(const QString &clientId, const QString &clientSecret);
    void authenticate();
    void refreshAccessToken(bool isInitialLogin = false);
    void signOut();
    void submitAuthCode(const QString &code);

    QString redirectUri() const { return m_redirectUri; }

signals:
    void authSuccess(const QString &message);
    void authFailed(const QString &error);
    void tokenRefreshed();
    void authCodeRequired(const QString &authUrl);

private:
    bool startCallbackServer();
    void onCallbackConnection();
    void handleCallbackRequest(QTcpSocket *socket, const QByteArray &requestHead);
    void sendHtmlResponse(QTcpSocket *socket);
    void exchangeCodeForTokens(const QString &code);
    void storeTokens(const QString &access, const QString &refresh, int expiresIn);
    void loadSavedTokens();
    QString buildAuthUrl() const;
    void stopCallbackServer();

    // --- PKCE & CSRF State (Item #1) ---
    void generatePkceAndState(QString &outChallenge, QString &outState);
    QString m_oauthState;
    QString m_codeVerifier;

    // --- Cross-Platform OS Vault Helpers (Item #2) ---
    void writeTokenToVault(const QString &key, const QString &value);
    QString readTokenFromVault(const QString &key);
    void deleteTokenFromVault(const QString &key);

    QNetworkAccessManager *m_net;
    QTcpServer *m_callbackServer = nullptr;
    QHash<QTcpSocket *, QByteArray> m_callbackBuffers;

    QString m_clientId;
    QString m_clientSecret;
    QString m_redirectUri;
    QStringList m_scopes;

    QString m_accessToken;
    QString m_refreshToken;
    int m_expiresIn = 0;
    QDateTime m_tokenAcquiredAt;
    bool m_isRefreshing = false;
};