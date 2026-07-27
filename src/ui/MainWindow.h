#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>
#include <QHBoxLayout>
#include "models/Conversation.h"

struct DriveFile;
class AuthManager;
class DriveApi;
class CopilotApi;
class ConversationModel;
class ConversationListPane;
class PromptListPane;
class ResponsePane;
class SearchDialog;
class LocalCache;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onSignIn();
    void onSignOut();
    void onFolderFound(const QString &folderId);
    void onConversationsLoaded(const QList<DriveFile> &files);
    void onConversationSelected(int index);
    void onPromptSelected(int pairIndex);
    void onSearchTriggered();
    void onToggleThoughts(bool visible);
    void onSortChanged(int index);
    void onBackendChanged(int index);
    void onSendPrompt();
    void onCopilotConnected(const QString &modelName);
    void onCopilotResponse(const QString &content, const QString &conversationId);
    void onCopilotError(const QString &error);

private:
    void setupUi();
    void setupToolbar();
    void setupStatusBar();
    void loadConversation(const QString &fileId, const QString &fileName);
    void switchToGoogleBackend();
    void switchToCopilotBackend();
    void addCopilotMessage(const QString &role, const QString &text);

    AuthManager *m_auth;
    DriveApi *m_drive;
    CopilotApi *m_copilot;
    ConversationModel *m_convModel;
    LocalCache *m_cache;

    ConversationListPane *m_convPane;
    PromptListPane *m_promptPane;
    ResponsePane *m_responsePane;
    SearchDialog *m_searchDlg;

    QSplitter *m_mainSplitter;
    QSplitter *m_leftSplitter;
    QToolBar *m_toolbar;

    QAction *m_signInAction;
    QAction *m_signOutAction;
    QAction *m_searchAction;
    QAction *m_toggleThoughts;
    QComboBox *m_sortCombo;
    QComboBox *m_backendCombo;
    QWidget *m_chatBar;
    QLineEdit *m_chatInput;
    QPushButton *m_sendButton;

    QString m_currentFolderId;
    QList<DriveFile> m_conversationFiles;
    int m_selectedConvIndex = -1;

    // Local (Copilot) conversation state
    QList<Conversation> m_localConversations;
    int m_currentLocalConvIndex = -1;
    QString m_currentConversationId;
    bool m_waitingForResponse = false;
};
