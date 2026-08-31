#include "ConversationListPane.h"
#include "app/Logger.h"
#include "api/DriveApi.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QAbstractItemView>
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

    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos)
            {
                QListWidgetItem *item = m_list->itemAt(pos);
                if (!item)
                    return;

                int row = m_list->row(item);
                if (row < 0 || row >= m_rowToConv.size())
                    return;

                int convIdx = m_rowToConv[row];
                const auto &conv = m_conversations[convIdx];

                QMenu menu(this);
                QAction *refreshAction = menu.addAction("Refresh");
                connect(refreshAction, &QAction::triggered, this, [this, conv]()
                        {
                            emit refreshRequested(conv.fileId, conv.fileName);
                        });
                menu.exec(m_list->mapToGlobal(pos));
            });
}

bool ConversationListPane::isValidConv(const Conversation &conv) const
{
    return true;
}

void ConversationListPane::setFileList(const QList<DriveFile> &files)
{
    Logger::instance().info(QString("Pane: setFileList called with %1 files").arg(files.size()));
    m_fileList = files;
    m_conversations.clear();
    for (const auto &f : files)
    {
        Conversation stub;
        stub.fileId = f.fileId;
        stub.fileName = f.fileName;
        stub.lastModified = f.modifiedTime;
        m_conversations.append(stub);
    }
    refreshList();
}

void ConversationListPane::addConversation(const Conversation &conv)
{
    Logger::instance().debug("Pane: addConversation called for: " + conv.fileName);
    bool found = false;
    for (int i = 0; i < m_conversations.size(); ++i)
    {
        if (m_conversations[i].fileId == conv.fileId)
        {
            // Update existing (e.g. replacing a stub with full data)
            m_conversations[i] = conv;
            found = true;
            break;
        }
    }

    if (!found) {
        m_conversations.append(conv);
    }

    // If we are in search mode and this is a new parent found via attachment,
    // it will be shown only if it's added to m_filterFileIds via setFilterFileIds
    refreshList();
}

void ConversationListPane::setFilterFileIds(const QSet<QString> &matchingFileIds)
{
    m_filterFileIds = matchingFileIds;
    m_hasFilter = true;
    refreshList();
}

void ConversationListPane::clearFilter()
{
    m_filterFileIds.clear();
    m_hasFilter = false;
    refreshList();
}

void ConversationListPane::clear()
{
    m_conversations.clear();
    m_rowToConv.clear();
    m_fileList.clear();
    m_filterFileIds.clear();
    m_hasFilter = false;
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

    for (int i = 0; i < m_conversations.size(); ++i)
    {
        const auto &conv = m_conversations[i];

        if (!isValidConv(conv)) continue;

        // Search mode filter: show ONLY matching conversations
        if (m_hasFilter && !m_filterFileIds.contains(conv.fileId))
            continue;

        m_rowToConv.append(i);
        QString title = conv.displayTitle();
        bool isDownloaded = !conv.model.isEmpty() || !conv.messages.isEmpty();

        QString dateStr;
        if (isDownloaded)
            dateStr = conv.lastMessageTime().isValid()
                          ? conv.lastMessageTime().toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
                          : "";
        else
            dateStr = conv.lastModified.isValid()
                          ? conv.lastModified.toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
                          : "";

        QString labelText = dateStr.isEmpty()
                                ? title
                                : QString("%1\n%2").arg(title, dateStr);

        auto *item = new QListWidgetItem(labelText);
        item->setToolTip(conv.fileName);

        if (isDownloaded)
        {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
        }
        else
        {
            item->setForeground(Qt::gray);
        }
        m_list->addItem(item);
    }

    m_list->blockSignals(false);
}

void ConversationListPane::applySort()
{
    auto sortTimeMs = [](const Conversation &c) -> qint64
    {
        QDateTime t = c.messages.isEmpty() ? c.lastModified : c.messages.last().createTime;
        return t.isValid() ? t.toMSecsSinceEpoch() : 0;
    };

    switch (m_sortMode)
    {
    case NewestFirst:
        std::sort(m_conversations.begin(), m_conversations.end(),
                  [sortTimeMs](const Conversation &a, const Conversation &b)
                  {
                      return sortTimeMs(a) > sortTimeMs(b);
                  });
        break;
    case OldestFirst:
        std::sort(m_conversations.begin(), m_conversations.end(),
                  [sortTimeMs](const Conversation &a, const Conversation &b)
                  {
                      return sortTimeMs(a) < sortTimeMs(b);
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

void ConversationListPane::selectByFileId(const QString &fileId)
{
    for (int row = 0; row < m_rowToConv.size(); ++row)
    {
        int convIdx = m_rowToConv[row];
        if (m_conversations[convIdx].fileId == fileId)
        {
            m_list->blockSignals(true);
            m_list->setCurrentRow(row);
            m_list->scrollToItem(m_list->item(row), QAbstractItemView::PositionAtCenter);
            m_list->blockSignals(false);
            emit conversationSelected(convIdx);
            return;
        }
    }
}

// File: src\ui\ConversationListPane.cpp
void ConversationListPane::removeConversation(const QString &fileId)
{
    Logger::instance().info("Pane: removeConversation called for ID: " + fileId);
    for (int i = 0; i < m_conversations.size(); ++i) {
        if (m_conversations[i].fileId == fileId) {
            m_conversations.removeAt(i);
            refreshList();
            break;
        }
    }
}