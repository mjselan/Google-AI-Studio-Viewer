#include "ConversationListPane.h"
#include <QVBoxLayout>
#include <QLabel>
#include <algorithm>

ConversationListPane::ConversationListPane(QWidget *parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *header = new QLabel("Conversations", this);
    header->setStyleSheet("font-weight: bold; padding: 4px;");
    layout->addWidget(header);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row)
    {
        if (row >= 0 && row < m_rowToConv.size())
        {
            int convIndex = m_rowToConv[row];
            emit conversationSelected(convIndex);
        }
    });
}

bool ConversationListPane::isValidConv(const Conversation &conv) const
{
    return !conv.model.isEmpty() || conv.totalTokenCount() > 0;
}

void ConversationListPane::setFileList(const QList<QPair<QString, QString>> &files)
{
    m_fileList = files;
}

void ConversationListPane::addConversation(const Conversation &conv)
{
    for (const auto &existing : m_conversations)
    {
        if (existing.fileId == conv.fileId)
        {
            return;
        }
    }

    m_conversations.append(conv);
    m_filesPopulated++;
    refreshList();
}

void ConversationListPane::clear()
{
    m_conversations.clear();
    m_rowToConv.clear();
    m_fileList.clear();
    m_list->clear();
    m_filesPopulated = 0;
}

int ConversationListPane::conversationCount() const
{
    return m_conversations.size();
}

const Conversation &ConversationListPane::conversationAt(int index) const
{
    return m_conversations.at(index);
}

QList<Conversation> ConversationListPane::allConversations() const
{
    return m_conversations;
}

void ConversationListPane::refreshList()
{
    // Sort before rendering so conversations always respect the selected mode
    applySort();

    m_list->blockSignals(true);
    m_list->clear();
    m_rowToConv.clear();

    for (int i = 0; i < m_conversations.size(); ++i)
    {
        const auto &conv = m_conversations[i];

        if (!isValidConv(conv))
        {
            continue;
        }

        m_rowToConv.append(i);

        QString title = conv.displayTitle();
        int tok = conv.totalTokenCount();
        QString modelShort = conv.model.section('/', -1);
        QString dateStr = conv.lastMessageTime().isValid()
            ? conv.lastMessageTime().toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
            : "";

        auto *item = new QListWidgetItem(
            QString("%1\n[%2] %3 tok  %4")
                .arg(title, modelShort)
                .arg(tok)
                .arg(dateStr));
        item->setToolTip(conv.fileName);
        m_list->addItem(item);
    }

    m_list->blockSignals(false);
}

void ConversationListPane::applySort()
{
    switch (m_sortMode)
    {
    case NewestFirst:
        std::sort(m_conversations.begin(), m_conversations.end(),
            [](const Conversation &a, const Conversation &b)
            {
                auto ta = a.messages.isEmpty() ? QDateTime() : a.messages.last().createTime;
                auto tb = b.messages.isEmpty() ? QDateTime() : b.messages.last().createTime;
                return ta > tb;
            });
        break;
    case OldestFirst:
        std::sort(m_conversations.begin(), m_conversations.end(),
            [](const Conversation &a, const Conversation &b)
            {
                auto ta = a.messages.isEmpty() ? QDateTime() : a.messages.last().createTime;
                auto tb = b.messages.isEmpty() ? QDateTime() : b.messages.last().createTime;
                return ta < tb;
            });
        break;
    case AlphaAZ:
        std::sort(m_conversations.begin(), m_conversations.end(),
            [](const Conversation &a, const Conversation &b)
            { return a.fileName.toLower() < b.fileName.toLower(); });
        break;
    case AlphaZA:
        std::sort(m_conversations.begin(), m_conversations.end(),
            [](const Conversation &a, const Conversation &b)
            { return a.fileName.toLower() > b.fileName.toLower(); });
        break;
    }
}

void ConversationListPane::sortBy(SortMode mode)
{
    m_sortMode = mode;
    applySort();
    refreshList();
}
