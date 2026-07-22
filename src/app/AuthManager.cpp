#include "AuthManager.h"
#include "Application.h"
#include "app/Logger.h"
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QSettings>
#include <QApplication>
#include <QFile>
#include <QDebug>

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

    // For the Google download format, force OOB redirect (our auth flow)
    m_redirectUri = obj["redirect_uri"].toString();
    if (m_redirectUri.isEmpty())
        m_redirectUri = "urn:ietf:wg:oauth:2.0:oob";

    m_scopes << "https://www.googleapis.com/auth/drive.readonly";

    loadSavedTokens();
}

void AuthManager::setCredentials(const QString &clientId, const QString &clientSecret)
{
    m_clientId = clientId;
    m_clientSecret = clientSecret;
    m_redirectUri = "urn:ietf:wg:oauth:2.0:oob";
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
        if (remainingSec > 60)
        {
            Logger::instance().info(
                QString("[AuthManager] Restored access token, expires in %1 sec")
                    .arg(remainingSec));
        }
        else
        {
            Logger::instance().info("[AuthManager] Saved access token expired, refreshing...");
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

    QString url = buildAuthUrl();
    QDesktopServices::openUrl(QUrl(url));
    emit authCodeRequired(url);
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

    Logger::instance().info("[AuthManager] Refreshing access token...");

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
                    Logger::instance().error("[AuthManager] " + errMsg);
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
                Logger::instance().info(
                    QString("[AuthManager] Token refreshed, expires in %1h %2m")
                        .arg(hours).arg(mins));
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
            Logger::instance().error("[AuthManager] " + errMsg);
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
        Logger::instance().info(
            QString("[AuthManager] Tokens obtained, expires in %1h %2m hasRefresh=%3")
                .arg(hours).arg(mins).arg(!refresh.isEmpty()));
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
