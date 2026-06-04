#include "ConversationListPane.h"
#include "app/Logger.h"
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
            Logger::instance().info(
                QString("Conversation selected: visual row=%1 convIndex=%2 name='%3'")
                    .arg(row).arg(convIndex)
                    .arg(m_conversations.value(convIndex).fileName));
            emit conversationSelected(convIndex);
        }
        else
        {
            Logger::instance().warn(
                QString("Conversation selection out of range: row=%1 mapping size=%2")
                    .arg(row).arg(m_rowToConv.size()));
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
    Logger::instance().info(QString("File list set: %1 files").arg(files.size()));
}

void ConversationListPane::addConversation(const Conversation &conv)
{
    for (const auto &existing : m_conversations)
    {
        if (existing.fileId == conv.fileId)
        {
            Logger::instance().debug(
                QString("Skipping duplicate conversation: '%1' (%2)")
                    .arg(conv.fileName, conv.fileId));
            return;
        }
    }

    m_conversations.append(conv);
    m_filesPopulated++;
    Logger::instance().debug(
        QString("Conversation added: '%1' model='%2' tokens=%3 pairs=%4")
            .arg(conv.fileName, conv.model)
            .arg(conv.totalTokenCount())
            .arg(conv.pairs.size()));
    refreshList();
}

void ConversationListPane::clear()
{
    m_conversations.clear();
    m_rowToConv.clear();
    m_fileList.clear();
    m_list->clear();
    m_filesPopulated = 0;
    Logger::instance().info("Conversation list cleared");
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
            Logger::instance().debug(
                QString("Skipping empty conversation at index %1: '%2'")
                    .arg(i).arg(conv.fileName));
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

    Logger::instance().debug(
        QString("List rendered: %1 visible of %2 total conversations")
            .arg(m_rowToConv.size()).arg(m_conversations.size()));

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
    Logger::instance().info(QString("Sorting conversations: mode=%1").arg(static_cast<int>(mode)));
    applySort();
    refreshList();
}
