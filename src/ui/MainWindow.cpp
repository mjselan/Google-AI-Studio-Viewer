#include "MainWindow.h"
#include "ConversationListPane.h"
#include "PromptListPane.h"
#include "ResponsePane.h"
#include "SearchDialog.h"
#include "app/AuthManager.h"
#include "api/DriveApi.h"
#include "api/CopilotApi.h"
#include "api/ConversationModel.h"
#include "models/Conversation.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_auth(new AuthManager(this))
    , m_drive(new DriveApi(this))
    , m_copilot(new CopilotApi(this))
    , m_convModel(new ConversationModel(this))
    , m_convPane(new ConversationListPane(this))
    , m_promptPane(new PromptListPane(this))
    , m_responsePane(new ResponsePane(this))
    , m_searchDlg(nullptr)
{
    setupUi();
    setupToolbar();
    setupStatusBar();

    connect(m_auth, &AuthManager::authSuccess, this, [this](const QString &message)
    {
        m_drive->setAccessToken(m_auth->accessToken());
        m_drive->findFolder("Google AI Studio");
        m_signInAction->setVisible(false);
        m_signOutAction->setVisible(true);
        statusBar()->showMessage(message);
    });

    connect(m_auth, &AuthManager::authFailed, this, [this](const QString &err)
    {
        statusBar()->showMessage("Auth failed: " + err);
        QMessageBox::warning(this, "Auth Error", err);
    });

    connect(m_drive, &DriveApi::folderFound, this, [this](const QString &folderId)
    {
        onFolderFound(folderId);
    });
    connect(m_drive, &DriveApi::folderNotFound, this, [this]()
    {
        statusBar()->showMessage("'Google AI Studio' folder not found in Drive");
    });
    connect(m_drive, &DriveApi::conversationListReady, this, &MainWindow::onConversationsLoaded);
    connect(m_drive, &DriveApi::conversationDownloaded, this,
        [this](const QString &fileId, const QString &fileName, const QJsonObject &data)
        {
            Conversation conv = m_convModel->parseConversation(fileId, fileName, data);
            m_convPane->addConversation(conv);
        });
    connect(m_drive, &DriveApi::apiError, this, [this](const QString &err)
    {
        statusBar()->showMessage("API error: " + err);
    });

    connect(m_drive, &DriveApi::tokenNeedsRefresh, this, [this]()
    {
        statusBar()->showMessage("Token expired. Refreshing...");
        m_auth->refreshAccessToken();
    });

    connect(m_auth, &AuthManager::tokenRefreshed, this, [this]()
    {
        m_drive->setAccessToken(m_auth->accessToken());
        m_drive->retryPendingRequests();
        statusBar()->showMessage("Token refreshed. Continuing...");
    });

    connect(m_convPane, &ConversationListPane::conversationSelected,
        this, &MainWindow::onConversationSelected);
    connect(m_promptPane, &PromptListPane::promptSelected,
        this, &MainWindow::onPromptSelected);

    connect(m_auth, &AuthManager::authCodeRequired, this,
        [this](const QString &authUrl)
        {
            bool ok;
            QString code = QInputDialog::getText(this, "Google Auth",
                "1. Sign in with the browser that opened.\n"
                "2. Copy the code shown by Google.\n"
                "3. Paste it here and press OK.\n\n"
                "If the browser didn't open, visit:\n" + authUrl,
                QLineEdit::Normal, QString(), &ok);
            if (ok && !code.trimmed().isEmpty())
                m_auth->submitAuthCode(code.trimmed());
        });

    connect(m_copilot, &CopilotApi::connected, this, &MainWindow::onCopilotConnected);
    connect(m_copilot, &CopilotApi::connectionFailed, this, [this](const QString &err)
    {
        statusBar()->showMessage("Local server connection failed: " + err);
    });
    connect(m_copilot, &CopilotApi::responseReceived, this, &MainWindow::onCopilotResponse);
    connect(m_copilot, &CopilotApi::errorOccurred, this, &MainWindow::onCopilotError);

    connect(m_sendButton, &QPushButton::clicked, this, &MainWindow::onSendPrompt);
    connect(m_chatInput, &QLineEdit::returnPressed, this, &MainWindow::onSendPrompt);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_chatBar = new QWidget(this);
    QHBoxLayout *chatLayout = new QHBoxLayout(m_chatBar);
    chatLayout->setContentsMargins(4, 2, 4, 2);
    m_chatInput = new QLineEdit(this);
    m_chatInput->setPlaceholderText("Type a message...");
    m_chatInput->setMinimumHeight(28);
    m_sendButton = new QPushButton("Send", this);
    m_sendButton->setFixedWidth(60);
    m_chatBar->setLayout(chatLayout);
    chatLayout->addWidget(m_chatInput);
    chatLayout->addWidget(m_sendButton);
    m_chatBar->setVisible(false);
    mainLayout->addWidget(m_chatBar);

    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_leftSplitter = new QSplitter(Qt::Vertical, this);

    m_leftSplitter->addWidget(m_convPane);
    m_leftSplitter->addWidget(m_promptPane);
    m_leftSplitter->setStretchFactor(0, 1);
    m_leftSplitter->setStretchFactor(1, 1);

    m_mainSplitter->addWidget(m_leftSplitter);
    m_mainSplitter->addWidget(m_responsePane);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);

    mainLayout->addWidget(m_mainSplitter);

    setCentralWidget(centralWidget);
}

void MainWindow::setupToolbar()
{
    m_toolbar = addToolBar("Main");
    m_toolbar->setMovable(false);

    m_backendCombo = new QComboBox(this);
    m_backendCombo->addItems({"Google AI Studio", "Local Server (Copilot)"});
    m_toolbar->addWidget(m_backendCombo);
    connect(m_backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onBackendChanged);

    m_toolbar->addSeparator();

    m_signInAction = m_toolbar->addAction("Sign In with Google");
    connect(m_signInAction, &QAction::triggered, this, &MainWindow::onSignIn);

    m_signOutAction = m_toolbar->addAction("Sign Out");
    m_signOutAction->setVisible(false);
    connect(m_signOutAction, &QAction::triggered, this, &MainWindow::onSignOut);

    m_toolbar->addSeparator();

    m_searchAction = m_toolbar->addAction("Search");
    connect(m_searchAction, &QAction::triggered, this, &MainWindow::onSearchTriggered);

    m_toggleThoughts = m_toolbar->addAction("Show Thoughts");
    m_toggleThoughts->setCheckable(true);
    m_toggleThoughts->setChecked(false);
    connect(m_toggleThoughts, &QAction::toggled, this, &MainWindow::onToggleThoughts);

    m_toolbar->addSeparator();

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItems({"Newest First", "Oldest First", "Alphabetical A-Z", "Alphabetical Z-A"});
    m_toolbar->addWidget(m_sortCombo);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onSortChanged);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready. Sign in to get started.");
}

void MainWindow::onBackendChanged(int index)
{
    if (index == 0)
        switchToGoogleBackend();
    else
        switchToCopilotBackend();
}

void MainWindow::switchToGoogleBackend()
{
    m_chatBar->setVisible(false);
    m_signInAction->setVisible(!m_auth->isAuthenticated());
    m_signOutAction->setVisible(m_auth->isAuthenticated());

    m_convPane->clear();
    m_promptPane->clear();
    m_responsePane->clear();

    if (m_auth->isAuthenticated())
    {
        m_drive->setAccessToken(m_auth->accessToken());
        m_drive->findFolder("Google AI Studio");
    }
    else
    {
        statusBar()->showMessage("Ready. Sign in to get started.");
    }
}

void MainWindow::switchToCopilotBackend()
{
    m_signInAction->setVisible(false);
    m_signOutAction->setVisible(false);
    m_chatBar->setVisible(true);
    m_chatInput->setFocus();

    m_convPane->clear();
    m_promptPane->clear();
    m_responsePane->clear();

    statusBar()->showMessage("Connecting to local server...");
    m_copilot->fetchModels();
}

void MainWindow::onCopilotConnected(const QString &modelName)
{
    statusBar()->showMessage("Connected to local server. Model: " + modelName);

    for (const auto &conv : m_localConversations)
        m_convPane->addConversation(conv);
}

void MainWindow::onCopilotResponse(const QString &content, const QString &conversationId)
{
    m_waitingForResponse = false;
    m_chatInput->setEnabled(true);
    m_sendButton->setEnabled(true);

    if (!conversationId.isEmpty())
        m_currentConversationId = conversationId;

    addCopilotMessage("model", content);

    if (m_currentLocalConvIndex >= 0)
    {
        auto &conv = m_localConversations[m_currentLocalConvIndex];
        m_convPane->addConversation(conv);
        m_promptPane->setConversation(conv);
    }

    statusBar()->showMessage("Response received.");
}

void MainWindow::onCopilotError(const QString &error)
{
    m_waitingForResponse = false;
    m_chatInput->setEnabled(true);
    m_sendButton->setEnabled(true);

    if (m_currentLocalConvIndex >= 0)
    {
        auto &conv = m_localConversations[m_currentLocalConvIndex];
        m_convPane->addConversation(conv);
        m_promptPane->setConversation(conv);
    }

    statusBar()->showMessage("Error: " + error);
}

void MainWindow::onSendPrompt()
{
    QString text = m_chatInput->text().trimmed();
    if (text.isEmpty() || m_waitingForResponse)
    {
        return;
    }

    m_chatInput->clear();
    m_chatInput->setEnabled(false);
    m_sendButton->setEnabled(false);
    m_waitingForResponse = true;

    if (m_currentLocalConvIndex < 0)
    {
        Conversation conv;
        conv.fileId = QString("local-%1").arg(QDateTime::currentMSecsSinceEpoch());
        conv.fileName = text.left(80);
        conv.model = "copilot";
        m_localConversations.append(conv);
        m_currentLocalConvIndex = m_localConversations.size() - 1;
        m_convPane->addConversation(conv);
    }

    addCopilotMessage("user", text);

    if (m_currentLocalConvIndex >= 0)
    {
        auto &conv = m_localConversations[m_currentLocalConvIndex];
        m_convPane->addConversation(conv);
        m_promptPane->setConversation(conv);
    }

    statusBar()->showMessage("Sending...");
    m_copilot->sendMessage(text, m_currentConversationId);
}

void MainWindow::addCopilotMessage(const QString &role, const QString &text)
{
    if (m_currentLocalConvIndex < 0)
        return;

    auto &conv = m_localConversations[m_currentLocalConvIndex];

    Message msg;
    msg.text = text;
    msg.role = role;
    msg.createTime = QDateTime::currentDateTime();
    conv.messages.append(msg);

    if (role == "user")
    {
        Conversation::PromptResponsePair pair;
        pair.prompt = msg;
        conv.pairs.append(pair);
    }
    else if (!conv.pairs.isEmpty())
    {
        conv.pairs.last().responses.append(msg);
    }
}

void MainWindow::onSignIn()
{
    QString userProfilePath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString configPath = userProfilePath + "/AI/oauth_config.json";

    if (QFileInfo::exists(configPath))
    {
        m_auth->loadConfig(configPath);
    }
    else
    {
        bool ok;
        QString clientId = QInputDialog::getText(this, "Google API Credentials",
            "oauth_config.json not found at:\n" + configPath + "\n\n"
            "Enter your Google Drive Client ID:",
            QLineEdit::Normal, QString(), &ok);
        if (!ok || clientId.trimmed().isEmpty())
            return;

        QString clientSecret = QInputDialog::getText(this, "Google API Credentials",
            "Enter your Google Drive Client Secret:",
            QLineEdit::Password, QString(), &ok);
        if (!ok || clientSecret.trimmed().isEmpty())
            return;

        m_auth->setCredentials(clientId.trimmed(), clientSecret.trimmed());

        QDir().mkpath(QFileInfo(configPath).absolutePath());
        QFile f(configPath);
        if (f.open(QIODevice::WriteOnly))
        {
            QJsonObject installed;
            installed["client_id"] = clientId.trimmed();
            installed["client_secret"] = clientSecret.trimmed();
            QJsonObject root;
            root["installed"] = installed;
            f.write(QJsonDocument(root).toJson());
            f.close();
        }
    }

    if (!m_auth->isAuthenticated())
        m_auth->authenticate();
    else
    {
        m_drive->setAccessToken(m_auth->accessToken());
        m_drive->findFolder("Google AI Studio");
    }
}

void MainWindow::onSignOut()
{
    m_auth->signOut();
    m_signInAction->setVisible(true);
    m_signOutAction->setVisible(false);
    m_convPane->clear();
    m_promptPane->clear();
    m_responsePane->clear();
    statusBar()->showMessage("Signed out.");
}

void MainWindow::onFolderFound(const QString &folderId)
{
    m_currentFolderId = folderId;
    statusBar()->showMessage("Loading conversations...");
    m_drive->listConversations(folderId);
}

void MainWindow::onConversationsLoaded(const QList<QPair<QString, QString>> &files)
{
    m_conversationFiles = files;
    //int count = qMin(2, files.size());
    int count = files.size();
    statusBar()->showMessage(QString("Found %1 conversations. Downloading last %2...")
        .arg(files.size()).arg(count));

    for (int i = 0; i < count; ++i)
    {
        m_drive->downloadConversation(files[i].first, files[i].second);
    }

    m_convPane->setFileList(files);
}

void MainWindow::onConversationSelected(int index)
{
    if (index < 0 || index >= m_convPane->conversationCount())
    {
        return;
    }

    m_selectedConvIndex = index;
    const auto &conv = m_convPane->conversationAt(index);

    m_promptPane->setConversation(conv);
    m_responsePane->clear();

    statusBar()->showMessage(
        QString("Conversation: %1 | %2 pairs | %3 tokens")
            .arg(conv.fileName)
            .arg(conv.pairs.size())
            .arg(conv.totalTokenCount()));
}

void MainWindow::onPromptSelected(int pairIndex)
{
    if (m_selectedConvIndex < 0)
    {
        return;
    }

    const auto &conv = m_convPane->conversationAt(m_selectedConvIndex);
    if (pairIndex < 0 || pairIndex >= conv.pairs.size())
    {
        return;
    }

    const auto &pair = conv.pairs.at(pairIndex);
    bool showThoughts = m_toggleThoughts->isChecked();

    m_responsePane->showResponse(pair, showThoughts, m_drive);
}

void MainWindow::onSearchTriggered()
{
    if (!m_searchDlg)
        m_searchDlg = new SearchDialog(m_convPane->allConversations(), this);

    m_searchDlg->show();
    m_searchDlg->raise();
    m_searchDlg->activateWindow();
}

void MainWindow::onToggleThoughts(bool visible)
{
    Q_UNUSED(visible)
    if (m_selectedConvIndex >= 0)
    {
        int promptIndex = m_promptPane->currentIndex();
        if (promptIndex >= 0)
            onPromptSelected(promptIndex);
    }
}

void MainWindow::onSortChanged(int index)
{
    switch (index)
    {
    case 0: m_convPane->sortBy(ConversationListPane::NewestFirst); break;
    case 1: m_convPane->sortBy(ConversationListPane::OldestFirst); break;
    case 2: m_convPane->sortBy(ConversationListPane::AlphaAZ); break;
    case 3: m_convPane->sortBy(ConversationListPane::AlphaZA); break;
    }
}

void MainWindow::loadConversation(const QString &fileId, const QString &fileName)
{
    m_drive->downloadConversation(fileId, fileName);
}
