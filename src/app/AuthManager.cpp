#include "AuthManager.h"
#include "Application.h"
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QSettings>
#include <QApplication>
#include <QFile>
#include <QDebug>
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

    // Support both root-level keys and the "installed" wrapper from Google Cloud download
    QJsonObject obj = doc.object();
    bool isGoogleFormat = obj.contains("installed");
    if (isGoogleFormat)
        obj = obj["installed"].toObject();

    m_clientId = obj["client_id"].toString();
    m_clientSecret = obj["client_secret"].toString();

    // Redirect URI is set to a local loopback URI at authenticate() time so
    // the auth code is captured automatically (Google has deprecated OOB flow).
    m_redirectUri.clear();

    m_scopes << "https://www.googleapis.com/auth/drive.readonly";

    loadSavedTokens();
}

void AuthManager::setCredentials(const QString &clientId, const QString &clientSecret)
{
    m_clientId = clientId;
    m_clientSecret = clientSecret;
    m_redirectUri.clear();
    m_scopes << "https://www.googleapis.com/auth/drive.readonly";

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

    // If we already have a valid access token, nothing to do
    if (!m_accessToken.isEmpty())
    {
        int remainingSec = m_expiresIn;
        if (m_tokenAcquiredAt.isValid())
        {
            qint64 elapsedSec = m_tokenAcquiredAt.secsTo(QDateTime::currentDateTimeUtc());
            remainingSec = m_expiresIn - static_cast<int>(elapsedSec);
        }
        int hours = remainingSec / 3600;
        int mins = (remainingSec % 3600) / 60;
        emit authSuccess(
            QString("Authenticated. Token expires in %1h %2m.").arg(hours).arg(mins));
        return;
    }

    // If we already have a refresh token stored from a previous authorization,
    // exchange it for a new access token rather than starting from scratch.
    if (!m_refreshToken.isEmpty())
    {
        refreshAccessToken();
        return;
    }

    // Start the local loopback callback server before opening the browser so
    // Google can redirect the auth code back to us automatically.
    if (!startCallbackServer())
        return;

    QString url = buildAuthUrl();
    QDesktopServices::openUrl(QUrl(url));
    emit authCodeRequired(url);
}

bool AuthManager::startCallbackServer()
{
    if (m_callbackServer && m_callbackServer->isListening())
        return true;

    if (!m_callbackServer)
        m_callbackServer = new QTcpServer(this);

    if (!m_callbackServer->listen(QHostAddress::LocalHost, 0))
    {
        emit authFailed("Could not start local OAuth callback server: "
                        + m_callbackServer->errorString());
        return false;
    }

    m_redirectUri = QString("http://127.0.0.1:%1/callback").arg(m_callbackServer->serverPort());
    connect(m_callbackServer, &QTcpServer::newConnection,
            this, &AuthManager::onCallbackConnection);
    return true;
}

void AuthManager::onCallbackConnection()
{
    while (m_callbackServer->hasPendingConnections())
    {
        QTcpSocket *socket = m_callbackServer->nextPendingConnection();
        socket->setParent(m_callbackServer);
        // Guaranteed cleanup: if the client disconnects before the HTTP header
        // is complete, drop its buffered data and delete the socket.
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
                return;  // request header not complete yet

            QByteArray head = buf.left(headerEnd);
            m_callbackBuffers.remove(socket);

            handleCallbackRequest(socket, head);
            socket->disconnectFromHost();
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

void AuthManager::refreshAccessToken()
{
    if (m_refreshToken.isEmpty())
    {
        emit authFailed("No refresh token available");
        return;
    }

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
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();
                auto responseBody = reply->readAll();
                if (reply->error() != QNetworkReply::NoError)
                {
                    // If the refresh token is invalid or expired, clear it so the
                    // user can start from scratch next time.
                    signOut();
                    QString errMsg = "Token refresh failed: " + reply->errorString();
                    auto doc = QJsonDocument::fromJson(responseBody);
                    if (!doc.isNull() && doc.object().contains("error"))
                        errMsg += "\n" + doc.object()["error_description"].toString();
                    emit authFailed(errMsg);
                    return;
                }
                auto doc = QJsonDocument::fromJson(responseBody);
                auto obj = doc.object();
                QString access = obj["access_token"].toString();
                int expiresIn = obj["expires_in"].toInt();
                storeTokens(access, QString(), expiresIn);
                int hours = expiresIn / 3600;
                int mins = (expiresIn % 3600) / 60;
                emit tokenRefreshed();
                emit authSuccess(
                    QString("Token refreshed. Expires in %1h %2m.").arg(hours).arg(mins));
            });
}

void AuthManager::signOut()
{
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
        auto doc = QJsonDocument::fromJson(responseBody);
        auto obj = doc.object();
        QString access = obj["access_token"].toString();
        QString refresh = obj["refresh_token"].toString();
        int expiresIn = obj["expires_in"].toInt();
        storeTokens(access, refresh, expiresIn);
        int hours = expiresIn / 3600;
        int mins = (expiresIn % 3600) / 60;
        emit authSuccess(
            QString("Authenticated. Token expires in %1h %2m.").arg(hours).arg(mins));
    });
}

void AuthManager::storeTokens(const QString &access, const QString &refresh, int expiresIn)
{
    m_accessToken = access;
    m_expiresIn = expiresIn;
    m_tokenAcquiredAt = QDateTime::currentDateTimeUtc();

    if (!refresh.isEmpty())
        m_refreshToken = refresh;

    // Always persist tokens
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
    query.addQueryItem("scope", m_scopes.join(" "));
    query.addQueryItem("access_type", "offline");
    query.addQueryItem("prompt", "consent");

    QUrl url("https://accounts.google.com/o/oauth2/v2/auth");
    url.setQuery(query);
    return url.toString();
}
