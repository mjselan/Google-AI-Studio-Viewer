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
            emit conversationSelected(convIndex);
        }
    });
}

bool ConversationListPane::isValidConv(const Conversation &conv) const
{
    return true;
}

void ConversationListPane::setFileList(const QList<QPair<QString, QString>> &files)
{
    Logger::instance().debug(QString("[ConvList] setFileList: received %1 files").arg(files.size()));
    m_fileList = files;
    m_conversations.clear();
    for (const auto &f : files)
    {
        Logger::instance().debug(QString("[ConvList] Creating stub: fileId=%1 fileName=%2").arg(f.first, f.second));
        Conversation stub;
        stub.fileId = f.first;
        stub.fileName = f.second;
        m_conversations.append(stub);
    }
    refreshList();
}

void ConversationListPane::addConversation(const Conversation &conv)
{
    Logger::instance().debug(QString("[ConvList] addConversation: fileId=%1 fileName=%2 model=%3 messages=%4 pairs=%5")
        .arg(conv.fileId, conv.fileName, conv.model)
        .arg(conv.messages.size())
        .arg(conv.pairs.size()));

    for (int i = 0; i < m_conversations.size(); ++i)
    {
        if (m_conversations[i].fileId == conv.fileId)
        {
            Logger::instance().debug(QString("[ConvList] Updating existing stub at index %1").arg(i));
            m_conversations[i] = conv;
            refreshList();
            return;
        }
    }

    Logger::instance().debug("[ConvList] Appending new conversation (not a stub)");
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
    applySort();

    m_list->blockSignals(true);
    m_list->clear();
    m_rowToConv.clear();

    Logger::instance().debug(QString("[ConvList] refreshList: rendering %1 conversations").arg(m_conversations.size()));

    for (int i = 0; i < m_conversations.size(); ++i)
    {
        const auto &conv = m_conversations[i];

        if (!isValidConv(conv)) continue;

        m_rowToConv.append(i);
        QString title = conv.displayTitle();
        bool isDownloaded = !conv.model.isEmpty() || !conv.messages.isEmpty();

        Logger::instance().debug(QString("[ConvList] Item %1 fileId=%2 title='%3' isDownloaded=%4 messages=%5 model='%6'")
            .arg(i)
            .arg(conv.fileId)
            .arg(title)
            .arg(isDownloaded ? "true" : "false")
            .arg(conv.messages.size())
            .arg(conv.model));

        QString labelText;
        if (isDownloaded)
        {
            int tok = conv.totalTokenCount();
            QString modelShort = conv.model.section('/', -1);
            QString dateStr = conv.lastMessageTime().isValid()
                ? conv.lastMessageTime().toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
                : "";

            labelText = QString("%1\n[%2] %3 tok  %4")
                            .arg(title, modelShort).arg(tok).arg(dateStr);
        }
        else
        {
            labelText = QString("%1\n[Click to load...]").arg(title);
        }

        Logger::instance().debug(QString("[ConvList] Label text: %1").arg(labelText));

        auto *item = new QListWidgetItem(labelText);
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
