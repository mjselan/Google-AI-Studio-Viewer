#pragma once

#include <QWidget>
#include <QListWidget>
#include <QList>
#include "models/Conversation.h"

struct DriveFile;

class ConversationListPane : public QWidget
{
    Q_OBJECT
public:
    enum SortMode { NewestFirst, OldestFirst, AlphaAZ, AlphaZA };

    explicit ConversationListPane(QWidget *parent = nullptr);

    void setFileList(const QList<DriveFile> &files);
    void addConversation(const Conversation &conv);
    void clear();
    int conversationCount() const;
    const Conversation &conversationAt(int index) const;
    QList<Conversation> allConversations() const;

    void sortBy(SortMode mode);

signals:
    void conversationSelected(int index);

private:
    void refreshList();
    void applySort();
    bool isValidConv(const Conversation &conv) const;

    QListWidget *m_list;
    QList<Conversation> m_conversations;
    QList<int> m_rowToConv;  // visual row → m_conversations index
    QList<DriveFile> m_fileList;
    SortMode m_sortMode = NewestFirst;
    int m_filesPopulated = 0;
};
