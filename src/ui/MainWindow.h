#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QComboBox>

class AuthManager;
class DriveApi;
class ConversationModel;
class ConversationListPane;
class PromptListPane;
class ResponsePane;
class SearchDialog;

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
    void onConversationsLoaded(const QList<QPair<QString, QString>> &files);
    void onConversationSelected(int index);
    void onPromptSelected(int pairIndex);
    void onSearchTriggered();
    void onToggleThoughts(bool visible);
    void onSortChanged(int index);

private:
    void setupUi();
    void setupToolbar();
    void setupStatusBar();
    void loadConversation(const QString &fileId, const QString &fileName);

    AuthManager *m_auth;
    DriveApi *m_drive;
    ConversationModel *m_convModel;

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

    QString m_currentFolderId;
    QList<QPair<QString, QString>> m_conversationFiles; // id, name
    int m_selectedConvIndex = -1;
};
