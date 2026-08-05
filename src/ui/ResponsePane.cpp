#include "ResponsePane.h"
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
#include <QUrl>

static QString markdownToHtml(const QString &markdown)
{
    QString md = markdown;
    md.replace(QRegularExpression("~~(.+?)~~"), "<s>\\1</s>");
    md.replace("- [x]", "- &#x2611;");
    md.replace("- [ ]", "- &#x2610;");

    QTextDocument doc;
    doc.setMarkdown(md);
    QString html = doc.toHtml();

    int bodyStart = html.indexOf("<body");
    if (bodyStart < 0)
        return html;
    bodyStart = html.indexOf(">", bodyStart) + 1;
    int bodyEnd = html.indexOf("</body>", bodyStart);
    if (bodyEnd < 0)
        return html;
    QString body = html.mid(bodyStart, bodyEnd - bodyStart);

    body.replace(QRegularExpression("<span[^>]*>"), "");
    body.replace("</span>", "");
    body.replace("<p style=\"-qt-paragraph-type:empty\"/>", "<p><br></p>");

    return body;
}

static QPixmap fitImageWidth(const QPixmap &src, int maxWidth = 640)
{
    if (src.width() <= maxWidth)
        return src;
    return src.scaledToWidth(maxWidth, Qt::SmoothTransformation);
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
    if (m_attachmentConn)
        disconnect(m_attachmentConn);
    m_attachmentConn = QMetaObject::Connection();

    m_browser->clear();

    m_browser->setHtml(
        "<h3>Prompt</h3><div style='padding:8px; margin:4px 0;'>"
        + markdownToHtml(pair.prompt.text) + "</div><hr>");

    if (!pair.prompt.driveDocumentId.isEmpty())
    {
        requestAttachment(pair.prompt.driveDocumentId, drive);
    }

    if (!pair.prompt.inlineImages.isEmpty())
    {
        for (int i = 0; i < pair.prompt.inlineImages.size(); ++i)
        {
            const auto &img = pair.prompt.inlineImages[i];
            appendInlineImage(img.data, img.mimeType, i);
        }
    }
    else if (!pair.prompt.inlineImageData.isEmpty())
    {
        appendInlineImage(pair.prompt.inlineImageData, pair.prompt.inlineImageMime, 0);
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
        html = "<div style='background:#fff3cd; border:1px solid #ffc107; border-radius:4px; padding:8px; margin:4px 0;'>"
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

void ResponsePane::requestAttachment(const QString &fileId, DriveApi *drive)
{
    if (!drive)
        return;

    m_browser->append(
        QString("<p><i>[Downloading attachment %1...]</i></p>").arg(fileId.toHtmlEscaped()));

    m_attachmentConn = connect(drive, &DriveApi::attachmentDownloaded, this,
                               [this, fileId](const QString &id, const QByteArray &data, const QString &mimeType)
                               {
                                   if (id == fileId)
                                       insertAttachmentInline(data, mimeType, fileId);
                               });

    drive->downloadAttachment(fileId);
}

void ResponsePane::insertAttachmentInline(const QByteArray &data, const QString &mimeType,
                                          const QString &fileId)
{
    if (mimeType.startsWith("image/"))
    {
        QPixmap pix;
        if (pix.loadFromData(data))
        {
            QString url = QString("attachment://%1").arg(fileId);
            m_browser->document()->addResource(QTextDocument::ImageResource, QUrl(url), fitImageWidth(pix));
            m_browser->append("<div style='text-align:center; margin:6px 0;'><img src='" + url + "'/></div>");
        }
        else
        {
            m_browser->append("<p><i>Attached image could not be decoded.</i></p>");
        }
    }
    else if (mimeType.startsWith("text/") || mimeType.contains("json") || mimeType.contains("javascript"))
    {
        m_browser->append("<h4>Attached file:</h4><pre>" + QString::fromUtf8(data).toHtmlEscaped() + "</pre>");
    }
    else
    {
        m_browser->append(
            QString("<p><i>Attachment downloaded (%1, %2 bytes) but cannot be previewed inline.</i></p>")
                .arg(mimeType.toHtmlEscaped()).arg(data.size()));
    }
}

void ResponsePane::appendInlineImage(const QString &base64data, const QString &mimeType, int index)
{
    QString base64 = base64data;
    if (base64.contains(","))
        base64 = base64.section(',', 1);
    base64 = base64.trimmed();

    QByteArray raw = QByteArray::fromBase64(base64.toUtf8());
    QPixmap pix;
    if (pix.loadFromData(raw))
    {
        QString url = QString("inlineimage://img_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(index);
        m_browser->document()->addResource(QTextDocument::ImageResource, QUrl(url), fitImageWidth(pix));
        m_browser->append("<div style='text-align:center; margin:6px 0;'><img src='" + url + "'/></div>");
    }
    else
    {
        m_browser->append("<p><i>[Inline image could not be decoded]</i></p>");
    }
}

void ResponsePane::clear()
{
    if (m_attachmentConn)
        disconnect(m_attachmentConn);
    m_attachmentConn = QMetaObject::Connection();
    m_browser->clear();
}