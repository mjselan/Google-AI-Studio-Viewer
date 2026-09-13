#include "MainWindow.h"
#include "ConversationListPane.h"
#include "PromptListPane.h"
#include "ResponsePane.h"
#include "app/AuthManager.h"
#include "api/DriveApi.h"
#include "api/CopilotApi.h"
#include "api/ConversationModel.h"
#include "api/LocalCache.h"
#include "api/conversationsyncmanager.h"
#include "models/Conversation.h"
#include "app/Logger.h"
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
#include <QMenu>
#include <QMenuBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_auth(new AuthManager(this))
    , m_drive(new DriveApi(this))
    , m_copilot(new CopilotApi(this))
    , m_convModel(new ConversationModel(this))
    , m_convPane(new ConversationListPane(this))
    , m_promptPane(new PromptListPane(this))
    , m_responsePane(new ResponsePane(this))
    , m_searchTimer(new QTimer(this))
{
    setupUi();
    setupToolbar();
    setupMenuBar();
    setupStatusBar();
    m_cache.init();

    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(400);
    m_syncMgr = new ConversationSyncManager(&m_cache, m_convModel, this);

    connect(m_auth, &AuthManager::authSuccess, this, [this](const QString &message)
            {
                m_drive->setAccessToken(m_auth->accessToken());
                m_drive->findFolder("Google AI Studio");
                m_signInAction->setVisible(false);
                m_signOutAction->setVisible(true);
                statusBar()->showMessage(message);
                m_syncMgr->setAccessToken(m_auth->accessToken());
            });

    connect(m_auth, &AuthManager::authFailed, this, [this](const QString &err)
            {
                m_drive->resetState(); // Item #4: Unblock DriveApi queue
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
                QDateTime modTime;
                for (const auto &f : m_conversationFiles)
                {
                    if (f.fileId == fileId)
                    {
                        modTime = f.modifiedTime;
                        break;
                    }
                }

                Conversation conv;
                if (m_convModel->ingestConversation(fileId, fileName, modTime, data, &m_cache, conv))
                {
                    m_convPane->addConversation(conv);

                    if (m_selectedFileId == fileId)
                    {
                        int idx = indexForFileId();
                        if (idx >= 0)
                            onConversationSelected(idx);
                    }
                }
                else
                {
                    m_convPane->removeConversation(fileId);
                    if (m_matchingFileIds.contains(fileId))
                    {
                        m_matchingFileIds.remove(fileId);
                        m_convPane->setFilterFileIds(m_matchingFileIds);
                    }
                }
            });
    connect(m_drive, &DriveApi::apiError, this, [this](const QString &err)
            {
                statusBar()->showMessage("API error: " + err);
            });

    connect(m_drive, &DriveApi::tokenNeedsRefresh, this, [this]()
            {
                m_auth->refreshAccessToken(false); // false = silent background refresh
            });

    connect(m_auth, &AuthManager::tokenRefreshed, this, [this]()
            {
                m_drive->setAccessToken(m_auth->accessToken());
                m_syncMgr->setAccessToken(m_auth->accessToken());
                m_drive->retryPendingRequests();

                // Show a brief, non-intrusive 2-second status message without resetting the view
                statusBar()->showMessage("Session renewed.", 2000);
            });
    connect(m_convPane, &ConversationListPane::conversationSelected,
            this, &MainWindow::onConversationSelected);
    connect(m_convPane, &ConversationListPane::refreshRequested, this,
            [this](const QString &fileId, const QString &fileName)
            {
                m_drive->downloadConversation(fileId, fileName);
            });
    connect(m_promptPane, &PromptListPane::promptSelected,
            this, &MainWindow::onPromptSelected);

    connect(m_auth, &AuthManager::authCodeRequired, this,
            [this](const QString &authUrl)
            {
                Q_UNUSED(authUrl)
                statusBar()->showMessage(
                    "Sign in in progress. Complete the authorization in your browser...");
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
    // This handles the "cloud lookup" when an attachment hit is found
    connect(m_drive, &DriveApi::parentConversationFound, this, [this](const QString &attId, const DriveFile &parent)
            {
                // Convert the parent DriveFile into a Conversation stub
                Conversation parentConv;
                parentConv.fileId = parent.fileId;
                parentConv.fileName = parent.fileName;
                parentConv.lastModified = parent.modifiedTime;

                // Add the parent to the pane
                m_convPane->addConversation(parentConv);

                // Ensure the parent ID is allowed through the search filter
                m_matchingFileIds.insert(parent.fileId);
                m_convPane->setFilterFileIds(m_matchingFileIds);
            });
    // Update the search results handler:
    // This handles the primary search results
    connect(m_drive, &DriveApi::searchResultsReady, this, [this](const QList<DriveFile> &results)
            {
                if (m_activeSearchTerm.isEmpty()) return;

                for (const auto &df : results)
                {
                    if (DriveApi::isConversationFile(df.fileName, df.mimeType, df.fileExtension))
                    {
                        m_matchingFileIds.insert(df.fileId);
                    }
                    else
                    {
                        // Attachment match from Google Drive (e.g. con_out.txt, MainWindow.cpp, image.png)
                        // 1. Look up parent conversation(s) referencing this attachment in local cache
                        QSet<QString> parentIds = m_cache.findConversationsByAttachmentId(df.fileId);
                        if (!parentIds.isEmpty())
                        {
                            for (const QString &pid : parentIds)
                            {
                                ensureConversationInPane(pid);
                                m_matchingFileIds.insert(pid);
                            }
                        }
                        else
                        {
                            // 2. Fallback to Drive parent lookup if not yet cached
                            m_drive->findParentConversation(df.fileId);
                        }
                    }
                }

                m_convPane->setFilterFileIds(m_matchingFileIds);
                statusBar()->showMessage(QString("Search complete: %1 matching conversation%2 for '%3'.")
                                             .arg(m_matchingFileIds.size())
                                             .arg(m_matchingFileIds.size() == 1 ? "" : "s")
                                             .arg(m_activeSearchTerm));
            });
    // Connect Sync Manager Signals
    connect(m_syncMgr, &ConversationSyncManager::syncProgress, this,
            [this](int completed, int total, const QString &fileName)
            {
                statusBar()->showMessage(QString("Syncing: %1/%2 (%3)")
                                             .arg(completed)
                                             .arg(total)
                                             .arg(fileName));
            });

    connect(m_syncMgr, &ConversationSyncManager::conversationSynced, this,
            [this](const Conversation &conv)
            {
                m_convPane->addConversation(conv);

                // If a search is currently active, check if this incoming conversation matches
                if (!m_activeSearchTerm.isEmpty())
                {
                    bool match = conv.displayTitle().contains(m_activeSearchTerm, Qt::CaseInsensitive);
                    if (!match)
                    {
                        for (const auto &msg : conv.messages)
                        {
                            if (msg.text.contains(m_activeSearchTerm, Qt::CaseInsensitive))
                            {
                                match = true;
                                break;
                            }
                        }
                    }
                    if (match)
                    {
                        m_matchingFileIds.insert(conv.fileId);
                        m_convPane->setFilterFileIds(m_matchingFileIds);
                    }
                }
            });

    connect(m_syncMgr, &ConversationSyncManager::conversationInvalid, this,
            [this](const QString &fileId)
            {
                m_convPane->removeConversation(fileId);
                if (m_matchingFileIds.contains(fileId))
                {
                    m_matchingFileIds.remove(fileId);
                    m_convPane->setFilterFileIds(m_matchingFileIds);
                }
            });

    connect(m_syncMgr, &ConversationSyncManager::syncFinished, this,
            [this](int downloaded, int upToDate)
            {
                statusBar()->showMessage(QString("Sync complete. %1 updated, %2 up to date.")
                                             .arg(downloaded).arg(upToDate));
            });

    connect(m_syncMgr, &ConversationSyncManager::tokenNeedsRefresh, this,
            [this]()
            {
                m_auth->refreshAccessToken(false); // false = silent background refresh
            });
    QString userProfilePath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString configPath = userProfilePath + "/AI/oauth_config.json";
    if (QFileInfo::exists(configPath))
    {
        m_auth->loadConfig(configPath);
        if (m_auth->canAutoAuthenticate())
        {
            m_auth->authenticate();
        }
    }
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
    connect(m_backendCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onBackendChanged);

    m_toolbar->addSeparator();

    m_signInAction = m_toolbar->addAction("Sign In with Google");
    connect(m_signInAction, &QAction::triggered, this, &MainWindow::onSignIn);

    m_signOutAction = m_toolbar->addAction("Sign Out");
    m_signOutAction->setVisible(false);
    connect(m_signOutAction, &QAction::triggered, this, &MainWindow::onSignOut);

    m_toolbar->addSeparator();

    // Inline Toolbar Search Bar
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText("Search conversations...");
    m_searchInput->setClearButtonEnabled(true);
    m_searchInput->setMaximumWidth(200);
    m_toolbar->addWidget(m_searchInput);

    // "Search all attachments" Checkbox
    m_includeAttachmentsCheck = new QCheckBox("Search all attachments", this);
    m_includeAttachmentsCheck->setToolTip("Include text files, images, PDFs, and code attachments in search results");
    m_toolbar->addWidget(m_includeAttachmentsCheck);

    connect(m_includeAttachmentsCheck, &QCheckBox::toggled, this, [this](bool)
            {
                onSearchInputChanged();
            });

    connect(m_searchInput, &QLineEdit::textChanged, this, [this](const QString &)
            {
                m_searchTimer->start();
            });
    connect(m_searchTimer, &QTimer::timeout, this, &MainWindow::onSearchInputChanged);

    m_toolbar->addSeparator();

    // "Show Attachments" Checkbox (checked by default)
    m_showAttachmentsCheck = new QCheckBox("Show Attachments", this);
    m_showAttachmentsCheck->setChecked(true);
    m_showAttachmentsCheck->setToolTip("Toggle displaying files and images inline in the prompt");
    m_toolbar->addWidget(m_showAttachmentsCheck);

    connect(m_showAttachmentsCheck, &QCheckBox::toggled, this, &MainWindow::onToggleAttachments);

    m_toggleThoughts = m_toolbar->addAction("Show Thoughts");
    m_toggleThoughts->setCheckable(true);
    m_toggleThoughts->setChecked(false);
    connect(m_toggleThoughts, &QAction::toggled, this, &MainWindow::onToggleThoughts);

    m_toolbar->addSeparator();

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItems({"Newest First", "Oldest First", "Alphabetical A-Z", "Alphabetical Z-A"});
    m_toolbar->addWidget(m_sortCombo);
    connect(m_sortCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::onSortChanged);
}

void MainWindow::onSearchInputChanged()
{
    QString query = m_searchInput->text().trimmed();
    m_activeSearchTerm = query;

    if (query.isEmpty())
    {
        m_matchingFileIds.clear();
        m_convPane->clearFilter();
        m_promptPane->setSearchTerm(QString());
        m_responsePane->setSearchTerm(QString());
        statusBar()->showMessage("Search filter cleared.");
        return;
    }

    if (query.length() < 2)
        return;

    m_promptPane->setSearchTerm(query);
    m_responsePane->setSearchTerm(query);

    m_matchingFileIds.clear();

    // 1. INSTANT LOCAL SEARCH (SQLite Cache: Prompts, Responses, Thoughts, Titles)
    QSet<QString> localMatches = m_cache.searchContent(query);
    for (const QString &fid : localMatches)
    {
        ensureConversationInPane(fid);
        m_matchingFileIds.insert(fid);
    }

    // 2. SEARCH LOCAL COPILOT CHATS
    for (const auto &conv : m_localConversations)
    {
        bool match = conv.displayTitle().contains(query, Qt::CaseInsensitive);
        if (!match)
        {
            for (const auto &msg : conv.messages)
            {
                if (msg.text.contains(query, Qt::CaseInsensitive))
                {
                    match = true;
                    break;
                }
            }
        }
        if (match)
        {
            m_matchingFileIds.insert(conv.fileId);
        }
    }

    // Apply immediate local filter results to the pane
    m_convPane->setFilterFileIds(m_matchingFileIds);
    statusBar()->showMessage(QString("Found %1 local match%2 for '%3'. Searching Drive...")
                                 .arg(m_matchingFileIds.size())
                                 .arg(m_matchingFileIds.size() == 1 ? "" : "es")
                                 .arg(query));

    // 3. REMOTE DRIVE SEARCH (for un-cached files and attachments like .txt, .cpp, .png)
    bool includeAttachments = m_includeAttachmentsCheck->isChecked();
    m_drive->searchConversations(query, includeAttachments);
}


void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready. Sign in to get started.");
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *deleteLog = fileMenu->addAction("Delete log file");
    connect(deleteLog, &QAction::triggered, this, [this]()
            {
                QString path = Logger::instance().logFilePath();
                Logger::instance().deleteLogFile();
                statusBar()->showMessage(path.isEmpty()
                                             ? "No log file to delete."
                                             : "Log file deleted: " + path);
            });
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
            f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
            QJsonObject installed;
            installed["client_id"] = clientId.trimmed();
            installed["client_secret"] = clientSecret.trimmed();
            QJsonObject root;
            root["installed"] = installed;
            f.write(QJsonDocument(root).toJson());
            f.close();
        }
    }

    // Always call authenticate() — it handles valid tokens, token refresh, and browser login
    m_auth->authenticate();
}

void MainWindow::onSignOut()
{
    m_syncMgr->cancelSync();
    m_drive->resetState(); // Item #4: Reset DriveApi state
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



void MainWindow::onConversationsLoaded(const QList<DriveFile> &files)
{
    m_conversationFiles = files;
    m_convPane->clear();
    m_convPane->setFileList(files);

    // 1. Immediately populate UI from SQLite cache for instant startup
    for (const auto &f : files)
    {
        QDateTime cachedModTime = m_cache.getModifiedTime(f.fileId);
        if (cachedModTime.isValid() && cachedModTime.toSecsSinceEpoch() == f.modifiedTime.toSecsSinceEpoch())
        {
            QJsonObject data = m_cache.loadConversation(f.fileId);
            if (ConversationModel::isLikelyConversation(data))
            {
                Conversation parsed = m_convModel->parseConversation(f.fileId, f.fileName, data);
                m_convPane->addConversation(parsed);
            }
        }
    }

    // 2. Start background sync for new/modified conversations
    m_syncMgr->startSync(files);
}

void MainWindow::onConversationSelected(int index)
{
    if (index < 0 || index >= m_convPane->conversationCount())
        return;

    const auto &conv = m_convPane->conversationAt(index);
    m_selectedFileId = conv.fileId;

    if (conv.model.isEmpty() && conv.messages.isEmpty())
    {
        QDateTime cachedModTime = m_cache.getModifiedTime(conv.fileId);
        if (cachedModTime.isValid() && cachedModTime.toSecsSinceEpoch() == conv.lastModified.toSecsSinceEpoch())
        {
            QJsonObject data = m_cache.loadConversation(conv.fileId);
            if (!data.isEmpty())
            {
                Conversation parsed = m_convModel->parseConversation(conv.fileId, conv.fileName, data);
                m_convPane->addConversation(parsed);

                m_promptPane->clear();
                m_responsePane->clear();

                int newIndex = indexForFileId();
                if (newIndex >= 0)
                    onConversationSelected(newIndex);
                return;
            }
        }

        statusBar()->showMessage("Downloading conversation data...");
        m_promptPane->clear();
        m_responsePane->clear();
        m_drive->downloadConversation(conv.fileId, conv.fileName);
        return;
    }

    // Handle raw attachment files (e.g. con_out.txt, image.png) gracefully in ResponsePane
    if (conv.messages.isEmpty() && !conv.fileId.isEmpty())
    {
        m_promptPane->clear();

        Conversation::PromptResponsePair pair;
        pair.prompt.text = conv.fileName;
        pair.prompt.driveDocumentId = conv.fileId;

        Conversation attConv = conv;
        attConv.pairs.append(pair);
        m_promptPane->setConversation(attConv);

        bool showThoughts = m_toggleThoughts->isChecked();
        m_responsePane->showResponse(pair, showThoughts, m_showAttachmentsCheck->isChecked(), m_drive);

        statusBar()->showMessage(QString("Attachment: %1").arg(conv.fileName));
        return;
    }

    m_promptPane->setConversation(conv);
    m_responsePane->clear();

    if (!conv.pairs.isEmpty())
        onPromptSelected(0);

    statusBar()->showMessage(
        QString("Conversation: %1 | %2 pairs | %3 tokens")
            .arg(conv.fileName)
            .arg(conv.pairs.size())
            .arg(conv.totalTokenCount()));
}


void MainWindow::onPromptSelected(int pairIndex)
{
    int idx = indexForFileId();
    if (idx < 0)
    {
        return;
    }

    const auto &conv = m_convPane->conversationAt(idx);
    if (pairIndex < 0 || pairIndex >= conv.pairs.size())
    {
        return;
    }

    const auto &pair = conv.pairs.at(pairIndex);
    bool showThoughts = m_toggleThoughts->isChecked();
    bool showAttachments = m_showAttachmentsCheck->isChecked();

    m_responsePane->showResponse(pair, showThoughts, showAttachments, m_drive);
}

void MainWindow::onToggleAttachments(bool checked)
{
    Q_UNUSED(checked)
    if (indexForFileId() >= 0)
    {
        int promptIndex = m_promptPane->currentIndex();
        if (promptIndex >= 0)
            onPromptSelected(promptIndex);
    }
}

void MainWindow::onToggleThoughts(bool visible)
{
    Q_UNUSED(visible)
    if (indexForFileId() >= 0)
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

int MainWindow::indexForFileId() const
{
    if (m_selectedFileId.isEmpty())
        return -1;

    for (int i = 0; i < m_convPane->conversationCount(); ++i)
    {
        if (m_convPane->conversationAt(i).fileId == m_selectedFileId)
            return i;
    }

    return -1;
}

void MainWindow::ensureConversationInPane(const QString &fileId)
{
    for (int i = 0; i < m_convPane->conversationCount(); ++i)
    {
        if (m_convPane->conversationAt(i).fileId == fileId && !m_convPane->conversationAt(i).messages.isEmpty())
        {
            return;
        }
    }

    QJsonObject data = m_cache.loadConversation(fileId);
    if (!data.isEmpty() && ConversationModel::isLikelyConversation(data))
    {
        QString fileName;
        for (const auto &f : m_conversationFiles)
        {
            if (f.fileId == fileId)
            {
                fileName = f.fileName;
                break;
            }
        }
        Conversation parsed = m_convModel->parseConversation(fileId, fileName, data);
        m_convPane->addConversation(parsed);
    }
}