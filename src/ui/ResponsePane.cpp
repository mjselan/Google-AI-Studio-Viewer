#include "ResponsePane.h"
#include "FilePreviewWidget.h"
#include "api/DriveApi.h"
#include "app/Logger.h"
#include <QPixmap>
#include <QScrollArea>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextDocument>
#include <QScrollBar>
#include <QRegularExpression>

// Convert Google AI markdown to clean HTML for QTextBrowser
static QString markdownToHtml(const QString &markdown)
{
    QString md = markdown;

    // --- Pre-process features Qt's markdown doesn't handle ---

    // Strikethrough: ~~text~~ → <s>text</s>
    md.replace(QRegularExpression("~~(.+?)~~"), "<s>\\1</s>");

    // Task list items: use Unicode ballot boxes
    md.replace("- [x]", "- &#x2611;");
    md.replace("- [ ]", "- &#x2610;");

    // Convert via Qt's markdown renderer
    QTextDocument doc;
    doc.setMarkdown(md);
    QString html = doc.toHtml();

    // Extract only <body> content, strip the outer html/head/body tags
    int bodyStart = html.indexOf("<body");
    if (bodyStart < 0)
        return html;
    bodyStart = html.indexOf(">", bodyStart) + 1;
    int bodyEnd = html.indexOf("</body>", bodyStart);
    if (bodyEnd < 0)
        return html;
    QString body = html.mid(bodyStart, bodyEnd - bodyStart);

    // Clean up Qt-specific cruft: remove excessive <span> font tags
    body.replace(QRegularExpression("<span[^>]*>"), "");
    body.replace("</span>", "");
    // QTextDocument adds <p style="..."> — keep the style but clean up orphans
    body.replace("<p style=\"-qt-paragraph-type:empty\"/>", "<p><br></p>");

    return body;
}

ResponsePane::ResponsePane(QWidget *parent)
    : QWidget(parent)
    , m_browser(new QTextBrowser(this))
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    auto *header = new QLabel("AI Response", this);
    header->setStyleSheet("font-weight: bold; padding: 4px;");
    m_layout->addWidget(header);
    m_layout->addWidget(m_browser);
}

void ResponsePane::showResponse(const Conversation::PromptResponsePair &pair,
                                bool showThoughts, DriveApi *drive)
{
    removeFilePreviews();
    m_browser->clear();

    Logger::instance().info(
        QString("[ResponsePane] Showing pair: text.len=%1 driveDocumentId='%2' responses=%3")
            .arg(pair.prompt.text.length())
            .arg(pair.prompt.driveDocumentId)
            .arg(pair.responses.size()));

    m_browser->setHtml(
        "<h3>Prompt</h3><div style='padding:8px; margin:4px 0;'>"
        + markdownToHtml(pair.prompt.text) + "</div><hr>");

    if (!pair.prompt.driveDocumentId.isEmpty())
    {
        Logger::instance().info(
            QString("[ResponsePane] driveDocumentId non-empty, calling appendFileAttachment"));
        appendFileAttachment(pair.prompt.driveDocumentId, drive);
    }

    if (!pair.prompt.inlineImageData.isEmpty())
    {
        Logger::instance().info(
            QString("[ResponsePane] inlineImage present, mime=%1 len=%2")
                .arg(pair.prompt.inlineImageMime)
                .arg(pair.prompt.inlineImageData.length()));
        appendInlineImage(pair.prompt.inlineImageData, pair.prompt.inlineImageMime);
    }

    for (const auto &resp : pair.responses)
    {
        appendModelResponse(resp.text, resp.isThought, showThoughts);
    }

    if (pair.responses.isEmpty())
        m_browser->append("<p><i>No response yet.</i></p>");

    m_browser->verticalScrollBar()->setValue(0);
}

void ResponsePane::appendModelResponse(const QString &text, bool isThought, bool showThoughts)
{
    if (isThought && !showThoughts)
        return;

    QString html;

    if (text.startsWith("ERROR: Cannot read"))
    {
        html = "<div style='background:#fff3cd; border:1px solid #ffc107; border-radius:4px; "
               "padding:8px; margin:4px 0;'>"
               "<b>&#x26A0; Image not supported:</b><br>" + markdownToHtml(text) + "</div>";
    }
    else if (isThought)
    {
        html = "<div style='background:#f0f0f0; border-left:3px solid #888; padding:8px; margin:4px 0;'>"
               "<i>Thought:</i><br>" + markdownToHtml(text) + "</div>";
    }
    else
    {
        html = "<div style='padding:8px; margin:4px 0;'>" + markdownToHtml(text) + "</div>";
    }

    m_browser->append(html);
}

void ResponsePane::appendFileAttachment(const QString &fileId, DriveApi *drive)
{
    Logger::instance().info(
        QString("[ResponsePane] appendFileAttachment called: fileId='%1' drive=%2")
            .arg(fileId).arg(drive ? "valid" : "null"));
    if (!drive) return;

    m_browser->append("<hr><b>Attached file:</b> " + fileId + "<br>");

    auto *preview = new FilePreviewWidget(fileId, this);
    m_layout->addWidget(preview);
    m_filePreviews.append(preview);

    QPointer<FilePreviewWidget> safePreview(preview);

    connect(drive, &DriveApi::attachmentDownloaded, this,
        [this, safePreview, fileId](const QString &id, const QByteArray &data, const QString &mimeType)
        {
            if (id == fileId && safePreview)
                safePreview->setContent(data, mimeType);
        });

    drive->downloadAttachment(fileId);
}

void ResponsePane::appendInlineImage(const QString &base64data, const QString &mimeType)
{
    QByteArray raw = QByteArray::fromBase64(base64data.toUtf8());
    QPixmap pix;
    if (pix.loadFromData(raw))
    {
        auto *preview = new FilePreviewWidget("[inline image]", this);
        preview->setContent(raw, mimeType);
        m_layout->addWidget(preview);
        m_filePreviews.append(preview);
        Logger::instance().info(
            QString("[ResponsePane] Inline image displayed: %1x%2")
                .arg(pix.width()).arg(pix.height()));
    }
    else
    {
        Logger::instance().error("[ResponsePane] Failed to decode inline image");
        m_browser->append("<p><i>[Inline image could not be decoded]</i></p>");
    }
}

void ResponsePane::removeFilePreviews()
{
    for (auto *w : m_filePreviews)
    {
        m_layout->removeWidget(w);
        w->deleteLater();
    }
    m_filePreviews.clear();
}

void ResponsePane::clear()
{
    removeFilePreviews();
    m_browser->clear();
}
