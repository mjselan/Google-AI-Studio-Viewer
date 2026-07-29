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
        emit promptSelected(row);
    });
}

void PromptListPane::setConversation(const Conversation &conv)
{
    m_currentConv = conv;
    refreshList();
}

void PromptListPane::clear()
{
    m_list->clear();
    m_currentConv = Conversation();
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
        else if (preview.isEmpty() && !pair.prompt.inlineImageData.isEmpty())
            preview = "[Inline image]";
        else if (preview.isEmpty())
            preview = "(empty prompt)";

        QString dateStr = pair.prompt.createTime.isValid()
            ? pair.prompt.createTime.toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
            : "no-time";

        auto *item = new QListWidgetItem(
            QString("%1\n[%2 tok]  %3")
                .arg(preview)
                .arg(pair.prompt.tokenCount)
                .arg(dateStr));
        m_list->addItem(item);
    }

    m_list->blockSignals(false);
}
