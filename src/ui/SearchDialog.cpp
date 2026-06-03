#include "SearchDialog.h"
#include <QVBoxLayout>
#include <QLabel>

SearchDialog::SearchDialog(const QList<Conversation> &conversations,
                           QWidget *parent)
    : QDialog(parent)
    , m_searchInput(new QLineEdit(this))
    , m_resultsList(new QListWidget(this))
    , m_conversations(conversations)
{
    setWindowTitle("Search Conversations");
    resize(500, 400);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Search across all conversations:", this));
    layout->addWidget(m_searchInput);
    layout->addWidget(m_resultsList);

    m_searchInput->setPlaceholderText("Type to search...");
    connect(m_searchInput, &QLineEdit::textChanged, this, &SearchDialog::onSearchTextChanged);
}

void SearchDialog::onSearchTextChanged(const QString &text)
{
    m_resultsList->clear();

    if (text.length() < 2)
        return;

    QString lower = text.toLower();

    for (const auto &conv : m_conversations)
    {
        for (const auto &msg : conv.messages)
        {
            if (msg.text.toLower().contains(lower))
            {
                QString preview = msg.text.left(120);
                if (msg.text.length() > 120)
                    preview += "...";

                auto *item = new QListWidgetItem(
                    QString("[%1] %2: %3")
                        .arg(conv.fileName, msg.role, preview));
                m_resultsList->addItem(item);
            }
        }
    }

    if (m_resultsList->count() == 0 && text.length() >= 2)
        m_resultsList->addItem("No matches found.");
}
