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

    auto *app = qobject_cast<Application *>(qApp);
    if (app)
    {
        QSettings &s = app->settings();
        m_refreshToken = s.value("auth/refresh_token").toString();
        if (!m_refreshToken.isEmpty())
            refreshAccessToken();
    }
}

void AuthManager::authenticate()
{
    if (m_clientId.isEmpty())
    {
        emit authFailed("Client ID not configured. Edit config/oauth_config.json");
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

    QUrl url("https://oauth2.googleapis.com/token");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery body;
    body.addQueryItem("client_id", m_clientId);
    body.addQueryItem("client_secret", m_clientSecret);
    body.addQueryItem("refresh_token", m_refreshToken);
    body.addQueryItem("grant_type", "refresh_token");

    auto *reply = m_net->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError)
                {
                    // If the refresh token is invalid or expired, clear it so the
                    // user can start from scratch next time.
                    signOut();
                    emit authFailed("Token refresh failed: " + reply->errorString());
                    return;
                }
                auto doc = QJsonDocument::fromJson(reply->readAll());
                auto obj = doc.object();
                m_accessToken = obj["access_token"].toString();
                m_expiresIn = obj["expires_in"].toInt();
                emit tokenRefreshed();
                emit authSuccess(); // Let the app know that authentication via refresh succeeded
            });
}

void AuthManager::signOut()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    auto *app = qobject_cast<Application *>(qApp);
    if (app)
    {
        QSettings &s = app->settings();
        s.remove("auth/refresh_token");
        s.sync();
    }
}

void AuthManager::exchangeCodeForTokens(const QString &code)
{
    QUrl url("https://oauth2.googleapis.com/token");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

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
        if (reply->error() != QNetworkReply::NoError)
        {
            emit authFailed("Token exchange failed: " + reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        auto obj = doc.object();
        QString access = obj["access_token"].toString();
        QString refresh = obj["refresh_token"].toString();
        int expiresIn = obj["expires_in"].toInt();
        storeTokens(access, refresh, expiresIn);
        emit authSuccess();
    });
}

void AuthManager::storeTokens(const QString &access, const QString &refresh, int expiresIn)
{
    m_accessToken = access;
    m_expiresIn = expiresIn;
    if (!refresh.isEmpty())
    {
        m_refreshToken = refresh;
        auto *app = qobject_cast<Application *>(qApp);
        if (app)
        {
            QSettings &s = app->settings();
            s.setValue("auth/refresh_token", refresh);
            s.sync();
        }
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
