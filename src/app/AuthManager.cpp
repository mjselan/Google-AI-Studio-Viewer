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
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QEventLoop>

// Cross-platform QtKeychain header
#if __has_include(<qt6keychain/keychain.h>)
#include <qt6keychain/keychain.h>
#elif __has_include(<keychain.h>)
#include <keychain.h>
#endif

static const char *KEYCHAIN_SERVICE = "GoogleAIViewer";

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

// --- Cross-Platform OS Vault (Item #2) ---

void AuthManager::writeTokenToVault(const QString &key, const QString &value)
{
    if (value.isEmpty()) return;

    QKeychain::WritePasswordJob job(QString::fromLatin1(KEYCHAIN_SERVICE));
    job.setAutoDelete(false);
    job.setKey(key);
    job.setTextData(value);

    bool finished = false;
    QEventLoop loop;
    connect(&job, &QKeychain::Job::finished, &loop, [&loop, &finished]() {
        finished = true;
        loop.quit();
    });
    job.start();
    if (!finished)
        loop.exec();

    if (job.error() != QKeychain::NoError)
    {
        Logger::instance().warn(QString("Failed to write %1 to OS keychain: %2")
                                    .arg(key, job.errorString()));
    }
}

QString AuthManager::readTokenFromVault(const QString &key)
{
    QKeychain::ReadPasswordJob job(QString::fromLatin1(KEYCHAIN_SERVICE));
    job.setAutoDelete(false);
    job.setKey(key);

    bool finished = false;
    QEventLoop loop;
    connect(&job, &QKeychain::Job::finished, &loop, [&loop, &finished]() {
        finished = true;
        loop.quit();
    });
    job.start();
    if (!finished)
        loop.exec();

    if (job.error() == QKeychain::NoError)
    {
        return job.textData();
    }
    return QString();
}

void AuthManager::deleteTokenFromVault(const QString &key)
{
    QKeychain::DeletePasswordJob job(QString::fromLatin1(KEYCHAIN_SERVICE));
    job.setAutoDelete(false);
    job.setKey(key);

    bool finished = false;
    QEventLoop loop;
    connect(&job, &QKeychain::Job::finished, &loop, [&loop, &finished]() {
        finished = true;
        loop.quit();
    });
    job.start();
    if (!finished)
        loop.exec();
}

void AuthManager::storeTokens(const QString &access, const QString &refresh, int expiresIn)
{
    m_accessToken = access;
    m_expiresIn = expiresIn;
    m_tokenAcquiredAt = QDateTime::currentDateTimeUtc();

    if (!refresh.isEmpty())
        m_refreshToken = refresh;

    // 1. Store sensitive tokens in OS Vault (Windows Credential Mgr / macOS Keychain / Linux Secret Service)
    writeTokenToVault("access_token", m_accessToken);
    if (!m_refreshToken.isEmpty())
        writeTokenToVault("refresh_token", m_refreshToken);

    // 2. Store non-sensitive expiry timestamps in standard QSettings
    auto *app = qobject_cast<Application *>(qApp);
    if (app)
    {
        QSettings &s = app->settings();
        s.setValue("auth/expires_in", m_expiresIn);
        s.setValue("auth/token_acquired_at", m_tokenAcquiredAt.toString(Qt::ISODate));

        // Clean up legacy plaintext tokens if they existed in QSettings
        s.remove("auth/access_token");
        s.remove("auth/refresh_token");
        s.sync();
    }
}

void AuthManager::loadSavedTokens()
{
    auto *app = qobject_cast<Application *>(qApp);
    if (!app)
        return;

    QSettings &s = app->settings();
    m_expiresIn = s.value("auth/expires_in").toInt();

    QString acquiredStr = s.value("auth/token_acquired_at").toString();
    if (!acquiredStr.isEmpty())
        m_tokenAcquiredAt = QDateTime::fromString(acquiredStr, Qt::ISODate);

    // 1. Read tokens from the OS Vault
    m_refreshToken = readTokenFromVault("refresh_token");
    m_accessToken = readTokenFromVault("access_token");

    // 2. Migration fallback: If not in vault yet, check legacy QSettings
    if (m_refreshToken.isEmpty() && s.contains("auth/refresh_token"))
    {
        m_refreshToken = s.value("auth/refresh_token").toString();
        m_accessToken = s.value("auth/access_token").toString();
        // Migrate to vault immediately
        if (!m_refreshToken.isEmpty())
            writeTokenToVault("refresh_token", m_refreshToken);
        if (!m_accessToken.isEmpty())
            writeTokenToVault("access_token", m_accessToken);
        s.remove("auth/refresh_token");
        s.remove("auth/access_token");
        s.sync();
    }

    // Check if loaded access token is expired
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

void AuthManager::signOut()
{
    stopCallbackServer();
    m_accessToken.clear();
    m_refreshToken.clear();
    m_expiresIn = 0;
    m_tokenAcquiredAt = QDateTime();

    // Delete credentials from OS vault
    deleteTokenFromVault("access_token");
    deleteTokenFromVault("refresh_token");

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

// --- PKCE & CSRF State (Item #1) ---

void AuthManager::generatePkceAndState(QString &outChallenge, QString &outState)
{
    // 1. Generate 32 secure random bytes for code_verifier
    QByteArray verifierBytes(32, Qt::Uninitialized);
    QRandomGenerator::securelySeeded().fillRange(
        reinterpret_cast<quint32 *>(verifierBytes.data()),
        verifierBytes.size() / sizeof(quint32));

    // RFC 7636: Base64URL without padding = 43 characters
    m_codeVerifier = QString::fromLatin1(
        verifierBytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    // 2. Compute SHA-256 code_challenge
    QByteArray hash = QCryptographicHash::hash(m_codeVerifier.toLatin1(), QCryptographicHash::Sha256);
    outChallenge = QString::fromLatin1(
        hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    // 3. Generate 24 random bytes for CSRF state nonce
    QByteArray stateBytes(24, Qt::Uninitialized);
    QRandomGenerator::securelySeeded().fillRange(
        reinterpret_cast<quint32 *>(stateBytes.data()),
        stateBytes.size() / sizeof(quint32));
    m_oauthState = QString::fromLatin1(
        stateBytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    outState = m_oauthState;
}

QString AuthManager::buildAuthUrl() const
{
    QByteArray hash = QCryptographicHash::hash(m_codeVerifier.toLatin1(), QCryptographicHash::Sha256);
    QString codeChallenge = QString::fromLatin1(
        hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    QUrlQuery query;
    query.addQueryItem("client_id", m_clientId);
    query.addQueryItem("redirect_uri", m_redirectUri);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("scope", m_scopes.join(' '));
    query.addQueryItem("access_type", "offline");
    query.addQueryItem("prompt", "consent");

    // PKCE & CSRF State
    query.addQueryItem("state", m_oauthState);
    query.addQueryItem("code_challenge", codeChallenge);
    query.addQueryItem("code_challenge_method", "S256");

    QUrl url("https://accounts.google.com/o/oauth2/v2/auth");
    url.setQuery(query);
    return url.toString();
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
        refreshAccessToken(true);
        return;
    }

    if (!startCallbackServer())
        return;

    QString challenge, state;
    generatePkceAndState(challenge, state);

    QString url = buildAuthUrl();
    QDesktopServices::openUrl(QUrl(url));
    emit authCodeRequired(url);
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

                emit tokenRefreshed();

                if (isInitialLogin)
                {
                    emit authSuccess("Signed in to Google AI Studio.");
                }
            });
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

    // PKCE verification
    body.addQueryItem("code_verifier", m_codeVerifier);

    auto *reply = m_net->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                m_codeVerifier.clear();
                m_oauthState.clear();

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
                emit authSuccess("Signed in to Google AI Studio.");
            });
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
    QString returnedState = query.queryItemValue("state");

    // CSRF state check
    if (returnedState.isEmpty() || returnedState != m_oauthState)
    {
        emit authFailed("OAuth security check failed: CSRF state mismatch.");
        sendHtmlResponse(socket);
        return;
    }

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