#include "PromptListPane.h"
#include "app/Logger.h"
#include <QVBoxLayout>
#include <QLabel>

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
        Logger::instance().debug(
            QString("Prompt selected: row=%1 of %2 pairs")
                .arg(row).arg(m_currentConv.pairs.size()));
        emit promptSelected(row);
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
    m_currentConv = Conversation();
    Logger::instance().debug("Prompts cleared");
}

int PromptListPane::currentIndex() const
{
    return m_list->currentRow();
}

void PromptListPane::refreshList()
{
    m_list->blockSignals(true);
    m_list->clear();

    for (int i = 0; i < m_currentConv.pairs.size(); ++i)
    {
        const auto &pair = m_currentConv.pairs[i];
        QString preview = pair.prompt.text.left(80);
        if (pair.prompt.text.length() > 80)
            preview += "...";
        if (preview.isEmpty() && !pair.prompt.driveDocumentId.isEmpty())
            preview = "[Uploaded file]";
        else if (preview.isEmpty())
            preview = "(empty prompt)";

        auto *item = new QListWidgetItem(
            QString("%1\n[%2 tok]")
                .arg(preview)
                .arg(pair.prompt.tokenCount));
        m_list->addItem(item);
    }

    Logger::instance().debug(
        QString("Prompts rendered: %1 items").arg(m_currentConv.pairs.size()));

    m_list->blockSignals(false);
}
