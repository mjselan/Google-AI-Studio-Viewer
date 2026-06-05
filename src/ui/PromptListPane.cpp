#include "PromptListPane.h"
#include "app/Logger.h"
#include <QVBoxLayout>
#include <QLabel>
#include <algorithm>

PromptListPane::PromptListPane(QWidget *parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *header = new QLabel("User Prompts", this);
    header->setStyleSheet("font-weight: bold; padding: 4px;");
    layout->addWidget(header);
    layout->addWidget(m_list);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row)
    {
        if (row < 0 || row >= m_rowToPair.size())
            return;
        int pairIndex = m_rowToPair[row];
        Logger::instance().debug(
            QString("Entry selected: row=%1 pairIndex=%2 of %3")
                .arg(row).arg(pairIndex).arg(m_currentConv.pairs.size()));
        emit promptSelected(pairIndex);
    });
}

void PromptListPane::setConversation(const Conversation &conv)
{
    m_currentConv = conv;
    Logger::instance().debug(
        QString("Setting prompts: %1 pairs for '%2'")
            .arg(conv.pairs.size())
            .arg(conv.fileName));
    refreshList();
}

void PromptListPane::clear()
{
    m_list->clear();
    m_rowToPair.clear();
    m_currentConv = Conversation();
    Logger::instance().debug("Prompts cleared");
}

int PromptListPane::currentIndex() const
{
    int row = m_list->currentRow();
    return (row >= 0 && row < m_rowToPair.size()) ? m_rowToPair[row] : -1;
}

void PromptListPane::refreshList()
{
    m_list->blockSignals(true);
    m_list->clear();
    m_rowToPair.clear();

    QList<Entry> entries;
    for (int i = 0; i < m_currentConv.pairs.size(); ++i)
    {
        const auto &pair = m_currentConv.pairs[i];

        Entry promptEntry;
        promptEntry.pairIndex = i;
        promptEntry.role = "USER";
        promptEntry.text = pair.prompt.text;
        promptEntry.tokenCount = pair.prompt.tokenCount;
        promptEntry.createTime = pair.prompt.createTime;
        entries.append(promptEntry);

        for (const auto &resp : pair.responses)
        {
            Entry respEntry;
            respEntry.pairIndex = i;
            respEntry.role = "MODEL";
            respEntry.text = resp.text;
            respEntry.tokenCount = resp.tokenCount;
            respEntry.createTime = resp.createTime;
            entries.append(respEntry);
        }
    }

    std::sort(entries.begin(), entries.end(),
        [](const Entry &a, const Entry &b)
        {
            if (a.createTime != b.createTime)
                return a.createTime < b.createTime;
            if (a.role != b.role)
                return a.role == "USER";
            return false;
        });

    for (const auto &entry : entries)
    {
        m_rowToPair.append(entry.pairIndex);

        QString preview = entry.text.left(80);
        if (entry.text.length() > 80)
            preview += "...";
        if (preview.isEmpty())
            preview = "(empty)";

        QString dateStr = entry.createTime.isValid()
            ? entry.createTime.toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
            : "";

        auto *item = new QListWidgetItem(
            QString("%1\n%2 [%3 tok]  %4")
                .arg(preview, entry.role)
                .arg(entry.tokenCount)
                .arg(dateStr));
        m_list->addItem(item);
    }

    Logger::instance().debug(
        QString("Prompts rendered: %1 entries from %2 pairs")
            .arg(entries.size()).arg(m_currentConv.pairs.size()));

    m_list->blockSignals(false);
}
