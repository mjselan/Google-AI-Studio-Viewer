#pragma once

#include <QWidget>
#include <QListWidget>
#include <QList>
#include <QSet>
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
    void setFilterFileIds(const QSet<QString> &matchingFileIds);
    void clearFilter();
    void clear();
    int conversationCount() const;
    const Conversation &conversationAt(int index) const;
    QList<Conversation> allConversations() const;

    void sortBy(SortMode mode);
    void selectByFileId(const QString &fileId);

signals:
    void conversationSelected(int index);
    void refreshRequested(const QString &fileId, const QString &fileName);

private:
    void refreshList();
    void applySort();
    bool isValidConv(const Conversation &conv) const;

    QListWidget *m_list;
    QList<Conversation> m_conversations;
    QList<int> m_rowToConv;
    QList<DriveFile> m_fileList;
    QSet<QString> m_filterFileIds;
    bool m_hasFilter = false;
    SortMode m_sortMode = NewestFirst;
    int m_filesPopulated = 0;
};