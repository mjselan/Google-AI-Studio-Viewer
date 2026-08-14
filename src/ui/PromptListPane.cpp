#include "PromptListPane.h"
#include "app/Logger.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTextDocument>
#include <QRegularExpression>
#include <QApplication>

class HtmlDelegate : public QStyledItemDelegate
{
public:
    explicit HtmlDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem options = option;
        initStyleOption(&options, index);

        painter->save();

        QStyle *style = options.widget ? options.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &options, painter, options.widget);

        QTextDocument doc;
        doc.setDefaultFont(options.font);

        bool isSelected = (options.state & QStyle::State_Selected);
        bool hasFocus = (options.state & QStyle::State_HasFocus) || (options.state & QStyle::State_Active);

        QPalette::ColorGroup group = hasFocus ? QPalette::Active : QPalette::Inactive;
        QColor textColor = options.palette.color(group, isSelected ? QPalette::HighlightedText : QPalette::Text);

        // Fix for Windows inactive selection (light grey background where HighlightedText becomes invisible white):
        if (isSelected && !hasFocus)
        {
            textColor = options.palette.color(QPalette::Inactive, QPalette::Text);
        }

        doc.setDefaultStyleSheet(QString("body, div, p { color: %1; }").arg(textColor.name()));
        doc.setHtml(options.text);

        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &options, options.widget);
        painter->translate(textRect.left() + 2, textRect.top() + 2);
        QRect clip(0, 0, textRect.width() - 4, textRect.height() - 4);
        doc.drawContents(painter, clip);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        QStyleOptionViewItem options = option;
        initStyleOption(&options, index);
        QTextDocument doc;
        doc.setDefaultFont(options.font);
        doc.setHtml(options.text);
        return QSize(qRound(doc.idealWidth()) + 10, qMax(44, qRound(doc.size().height()) + 8));
    }
};

static QString highlightSearchHits(const QString &srcText, const QString &term)
{
    if (term.trimmed().isEmpty())
        return srcText.toHtmlEscaped();

    QString escapedSrc = srcText.toHtmlEscaped();
    QString escapedTerm = term.trimmed().toHtmlEscaped();

    QRegularExpression re(QRegularExpression::escape(escapedTerm),
                          QRegularExpression::CaseInsensitiveOption);
    return escapedSrc.replace(re, "<b>\\0</b>");
}

PromptListPane::PromptListPane(QWidget *parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *header = new QLabel("User Prompts", this);
    header->setStyleSheet("font-weight: bold; padding: 4px;");
    layout->addWidget(header);

    m_list->setItemDelegate(new HtmlDelegate(m_list));
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

void PromptListPane::setSearchTerm(const QString &term)
{
    m_searchTerm = term.trimmed();
    refreshList();
}

void PromptListPane::clear()
{
    m_list->clear();
    m_currentConv = Conversation();
    m_searchTerm.clear();
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

        QString previewHtml = highlightSearchHits(preview, m_searchTerm);
        previewHtml.replace("\n", "<br>");

        QString dateStr = pair.prompt.createTime.isValid()
                              ? pair.prompt.createTime.toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
                              : "no-time";

        QString itemHtml = QString("<div>%1</div><div style='font-size:11px; opacity:0.75;'>[%2 tok]  %3</div>")
                               .arg(previewHtml)
                               .arg(pair.prompt.tokenCount)
                               .arg(dateStr);

        auto *item = new QListWidgetItem(itemHtml);
        m_list->addItem(item);
    }

    m_list->blockSignals(false);
}