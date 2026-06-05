#pragma once

#include <QWidget>
#include <QListWidget>
#include <QList>
#include "models/Conversation.h"

class PromptListPane : public QWidget
{
    Q_OBJECT
public:
    explicit PromptListPane(QWidget *parent = nullptr);

    void setConversation(const Conversation &conv);
    void clear();
    int currentIndex() const;

signals:
    void promptSelected(int pairIndex);

private:
    struct Entry
    {
        int pairIndex;
        QString role;
        QString text;
        int tokenCount;
        QDateTime createTime;
    };

    void refreshList();

    QListWidget *m_list;
    Conversation m_currentConv;
    QList<int> m_rowToPair;
};
