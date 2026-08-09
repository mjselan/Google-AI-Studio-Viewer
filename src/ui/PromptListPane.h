#pragma once

#include <QWidget>
#include <QListWidget>
#include "models/Conversation.h"

class PromptListPane : public QWidget
{
    Q_OBJECT
public:
    explicit PromptListPane(QWidget *parent = nullptr);

    void setConversation(const Conversation &conv);
    void setSearchTerm(const QString &term);
    void clear();
    int currentIndex() const;

signals:
    void promptSelected(int pairIndex);

private:
    void refreshList();

    QListWidget *m_list;
    Conversation m_currentConv;
    QString m_searchTerm;
};