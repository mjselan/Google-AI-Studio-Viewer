#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include "models/Conversation.h"

class SearchDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SearchDialog(const QList<Conversation> &conversations,
                          QWidget *parent = nullptr);

private slots:
    void onSearchTextChanged(const QString &text);

private:
    QLineEdit *m_searchInput;
    QListWidget *m_resultsList;
    QList<Conversation> m_conversations;
};
