#include "AuthManager.h"
#include "Application.h"
#include "Logger.h"
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QSettings>
#include <QApplication>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>

AuthManager::AuthManager(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

bool AuthManager::isAuthenticated() const
{
    return !m_accessToken.isEmpty();
}

QString AuthManager::accessToken() const { return m_accessToken; }

int AuthManager::expiresIn() const { return m_expiresIn; }

void AuthManager::loadConfig(const QString &configPath)
{
    QFile f(configPath);
    if (!f.open(QIODevice::ReadOnly))
    {
        emit authFailed("Could not open config file: " + configPath);
        return;
    }
    auto doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    QJsonObject obj = doc.object();
    if (obj.contains("installed"))
        obj = obj["installed"].toObject();

    m_clientId = obj["client_id"].toString();
    m_clientSecret = obj["client_secret"].toString();
    m_redirectUri.clear();

    // Set explicitly to prevent duplicate accumulative appends
    m_scopes = { QStringLiteral("https://www.googleapis.com/auth/drive.readonly") };

    loadSavedTokens();
}

void AuthManager::setCredentials(const QString &clientId, const QString &clientSecret)
{
    m_clientId = clientId;
    m_clientSecret = clientSecret;
    m_redirectUri.clear();
    m_scopes = { QStringLiteral("https://www.googleapis.com/auth/drive.readonly") };

    loadSavedTokens();
}

void AuthManager::loadSavedTokens()
{
    auto *app = qobject_cast<Application *>(qApp);
    if (!app)
        return;

    QSettings &s = app->settings();
    m_refreshToken = s.value("auth/refresh_token").toString();
    m_accessToken = s.value("auth/access_token").toString();
    m_expiresIn = s.value("auth/expires_in").toInt();

    QString acquiredStr = s.value("auth/token_acquired_at").toString();
    if (!acquiredStr.isEmpty())
        m_tokenAcquiredAt = QDateTime::fromString(acquiredStr, Qt::ISODate);

    if (!m_accessToken.isEmpty() && m_tokenAcquiredAt.isValid())
    {
        qint64 elapsedSec = m_tokenAcquiredAt.secsTo(QDateTime::currentDateTimeUtc());
        int remainingSec = m_expiresIn - static_cast<int>(elapsedSec);
        if (remainingSec <= 60)
        {
            m_accessToken.clear();
        }
    }
}

void AuthManager::authenticate()
{
    if (m_clientId.isEmpty())
    {
        emit authFailed("Client ID not configured. Edit config/oauth_config.json");
        return;
    }

    if (!m_accessToken.isEmpty())
    {
        emit authSuccess("Signed in.");
        return;
    }

    if (!m_refreshToken.isEmpty())
    {
        // Treat as initial sign-in via saved refresh token
        refreshAccessToken(true);
        return;
    }

    if (!startCallbackServer())
        return;

    QString url = buildAuthUrl();
    QDesktopServices::openUrl(QUrl(url));
    emit authCodeRequired(url);
}

bool AuthManager::startCallbackServer()
{
    if (!m_callbackServer)
    {
        m_callbackServer = new QTcpServer(this);
        connect(m_callbackServer, &QTcpServer::newConnection,
                this, &AuthManager::onCallbackConnection);
    }

    if (m_callbackServer->isListening())
        return true;

    if (!m_callbackServer->listen(QHostAddress::LocalHost, 0))
    {
        emit authFailed("Could not start local OAuth callback server: "
                        + m_callbackServer->errorString());
        return false;
    }

    m_redirectUri = QString("http://127.0.0.1:%1/callback").arg(m_callbackServer->serverPort());
    return true;
}

void AuthManager::stopCallbackServer()
{
    if (m_callbackServer && m_callbackServer->isListening())
    {
        m_callbackServer->close();
    }
    m_callbackBuffers.clear();
}

void AuthManager::onCallbackConnection()
{
    while (m_callbackServer->hasPendingConnections())
    {
        QTcpSocket *socket = m_callbackServer->nextPendingConnection();
        socket->setParent(m_callbackServer);

        connect(socket, &QTcpSocket::disconnected, this, [this, socket]()
                {
                    m_callbackBuffers.remove(socket);
                    socket->deleteLater();
                });

        connect(socket, &QTcpSocket::readyRead, this, [this, socket]()
                {
                    m_callbackBuffers[socket].append(socket->readAll());
                    QByteArray &buf = m_callbackBuffers[socket];

                    int headerEnd = buf.indexOf("\r\n\r\n");
                    if (headerEnd < 0)
                        headerEnd = buf.indexOf("\n\n");
                    if (headerEnd < 0)
                        return;

                    QByteArray head = buf.left(headerEnd);
                    m_callbackBuffers.remove(socket);

                    handleCallbackRequest(socket, head);
                });
    }
}

void AuthManager::handleCallbackRequest(QTcpSocket *socket, const QByteArray &requestHead)
{
    QString firstLine = QString::fromLatin1(requestHead).split('\n').first();
    QString target = firstLine.section(' ', 1, 1);

    QUrlQuery query(QUrl(QStringLiteral("http://localhost") + target).query());
    QString error = query.queryItemValue("error");
    QString code = query.queryItemValue("code");

    if (!error.isEmpty())
        emit authFailed("OAuth error from provider: " + error);
    else if (!code.isEmpty())
        submitAuthCode(code);
    else
        emit authFailed("OAuth callback received no authorization code.");

    sendHtmlResponse(socket);
}

void AuthManager::sendHtmlResponse(QTcpSocket *socket)
{
    const QByteArray body =
        "<html><body><h3>Authorization complete</h3>"
        "<p>You can close this window and return to the application.</p>"
        "</body></html>";
    QByteArray response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                                            "Connection: close\r\n\r\n" + body;
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void AuthManager::submitAuthCode(const QString &code)
{
    exchangeCodeForTokens(code);
}

void AuthManager::refreshAccessToken(bool isInitialLogin)
{
    if (m_isRefreshing)
    {
        Logger::instance().info("Token refresh already in progress, skipping duplicate request.");
        return;
    }

    if (m_refreshToken.isEmpty())
    {
        emit authFailed("No refresh token available");
        return;
    }

    m_isRefreshing = true;

    QUrl url("https://oauth2.googleapis.com/token");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QUrlQuery body;
    body.addQueryItem("client_id", m_clientId);
    body.addQueryItem("client_secret", m_clientSecret);
    body.addQueryItem("refresh_token", m_refreshToken);
    body.addQueryItem("grant_type", "refresh_token");

    auto *reply = m_net->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply, isInitialLogin]()
            {
                reply->deleteLater();
                m_isRefreshing = false;

                auto responseBody = reply->readAll();
                if (reply->error() != QNetworkReply::NoError)
                {
                    auto doc = QJsonDocument::fromJson(responseBody);
                    QString oauthError = doc.object()["error"].toString();

                    // Only revoke credentials if Google explicitly rejects the refresh token (e.g. invalid_grant)
                    // If it was just a network timeout/offline error, keep the tokens so it can retry later.
                    if (oauthError == "invalid_grant" || oauthError == "unauthorized_client")
                    {
                        signOut();
                        emit authFailed("Session expired. Please sign in again.");
                    }
                    else
                    {
                        emit authFailed("Network error refreshing token: " + reply->errorString());
                    }
                    return;
                }

                auto doc = QJsonDocument::fromJson(responseBody);
                auto obj = doc.object();
                QString access = obj["access_token"].toString();
                int expiresIn = obj["expires_in"].toInt();
                storeTokens(access, QString(), expiresIn);

                Logger::instance().info(QString("OAuth token renewed. Valid for %1 seconds.").arg(expiresIn));

                // Always notify services (DriveApi, SyncMgr) that new token is ready
                emit tokenRefreshed();

                // ONLY notify UI to do full startup/folder listing on initial login!
                if (isInitialLogin)
                {
                    emit authSuccess("Signed in to Google AI Studio.");
                }
            });
}

void AuthManager::signOut()
{
    stopCallbackServer();
    m_accessToken.clear();
    m_refreshToken.clear();
    m_expiresIn = 0;
    m_tokenAcquiredAt = QDateTime();
    auto *app = qobject_cast<Application *>(qApp);
    if (app)
    {
        QSettings &s = app->settings();
        s.remove("auth/refresh_token");
        s.remove("auth/access_token");
        s.remove("auth/expires_in");
        s.remove("auth/token_acquired_at");
        s.sync();
    }
}

void AuthManager::exchangeCodeForTokens(const QString &code)
{
    QUrl url("https://oauth2.googleapis.com/token");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QUrlQuery body;
    body.addQueryItem("code", code);
    body.addQueryItem("client_id", m_clientId);
    body.addQueryItem("client_secret", m_clientSecret);
    body.addQueryItem("redirect_uri", m_redirectUri);
    body.addQueryItem("grant_type", "authorization_code");

    auto *reply = m_net->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();
                auto responseBody = reply->readAll();
                if (reply->error() != QNetworkReply::NoError)
                {
                    QString errMsg = "Token exchange failed: " + reply->errorString();
                    auto doc = QJsonDocument::fromJson(responseBody);
                    if (!doc.isNull() && doc.object().contains("error"))
                        errMsg += "\n" + doc.object()["error_description"].toString();
                    emit authFailed(errMsg);
                    return;
                }

                stopCallbackServer();

                auto doc = QJsonDocument::fromJson(responseBody);
                auto obj = doc.object();
                QString access = obj["access_token"].toString();
                QString refresh = obj["refresh_token"].toString();
                int expiresIn = obj["expires_in"].toInt();
                storeTokens(access, refresh, expiresIn);
                emit authSuccess(formatExpiryMessage("Authenticated.", expiresIn));
            });
}

void AuthManager::storeTokens(const QString &access, const QString &refresh, int expiresIn)
{
    m_accessToken = access;
    m_expiresIn = expiresIn;
    m_tokenAcquiredAt = QDateTime::currentDateTimeUtc();

    if (!refresh.isEmpty())
        m_refreshToken = refresh;

    auto *app = qobject_cast<Application *>(qApp);
    if (app)
    {
        QSettings &s = app->settings();
        s.setValue("auth/access_token", m_accessToken);
        s.setValue("auth/expires_in", m_expiresIn);
        s.setValue("auth/token_acquired_at", m_tokenAcquiredAt.toString(Qt::ISODate));
        if (!refresh.isEmpty())
            s.setValue("auth/refresh_token", m_refreshToken);
        s.sync();
    }
}

QString AuthManager::buildAuthUrl() const
{
    QUrlQuery query;
    query.addQueryItem("client_id", m_clientId);
    query.addQueryItem("redirect_uri", m_redirectUri);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("scope", m_scopes.join(' '));
    query.addQueryItem("access_type", "offline");
    query.addQueryItem("prompt", "consent");

    QUrl url("https://accounts.google.com/o/oauth2/v2/auth");
    url.setQuery(query);
    return url.toString();
}

QString AuthManager::formatExpiryMessage(const QString &prefix, int expiresInSec) const
{
    int hours = expiresInSec / 3600;
    int mins = (expiresInSec % 3600) / 60;
    return QString("%1 Token expires in %2h %3m.").arg(prefix).arg(hours).arg(mins);
}