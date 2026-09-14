#include "ResponsePane.h"
#include "api/DriveApi.h"
#include "app/Logger.h"
#include <QPixmap>
#include <QScrollArea>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextDocument>
#include <QTextCursor>
#include <QScrollBar>
#include <QRegularExpression>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QGuiApplication>

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
    m_browser->setContextMenuPolicy(Qt::CustomContextMenu);
    m_browser->viewport()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_browser, &QWidget::customContextMenuRequested,
            this, &ResponsePane::onCustomContextMenuRequested);
    connect(m_browser->viewport(), &QWidget::customContextMenuRequested,
            this, &ResponsePane::onCustomContextMenuRequested);
}

void ResponsePane::setSearchTerm(const QString &term)
{
    m_searchTerm = term.trimmed();
}

QString ResponsePane::applySearchHighlight(const QString &html) const
{
    if (m_searchTerm.isEmpty())
        return html;

    QString escapedTerm = m_searchTerm.toHtmlEscaped();
    QRegularExpression re(QString("(<[^>]*>)|(%1)").arg(QRegularExpression::escape(escapedTerm)),
                          QRegularExpression::CaseInsensitiveOption);

    QString result;
    int lastIndex = 0;
    auto it = re.globalMatch(html);
    while (it.hasNext())
    {
        auto match = it.next();
        result.append(html.mid(lastIndex, match.capturedStart() - lastIndex));
        if (match.capturedStart(1) != -1)
        {
            result.append(match.captured(1));
        }
        else
        {
            result.append("<mark style='background:#ffe066; color:#000;'>" + match.captured(2) + "</mark>");
        }
        lastIndex = match.capturedEnd();
    }
    result.append(html.mid(lastIndex));
    return result;
}

void ResponsePane::showResponse(const Conversation::PromptResponsePair &pair,
                                bool showThoughts, bool showAttachments, DriveApi *drive)
{
    m_currentPair = pair;
    m_showThoughts = showThoughts;
    m_showAttachments = showAttachments;
    m_drive = drive;

    QString docId = pair.prompt.driveDocumentId;
    // Only trigger download if attachments are enabled
    if (m_showAttachments && !docId.isEmpty() && !m_attachmentCache.contains(docId) && !m_pendingDownloads.contains(docId))
    {
        requestAttachment(docId, drive);
    }

    render();
}

void ResponsePane::render()
{
    m_browser->clear();
    m_attachmentStartPos = -1;
    m_attachmentEndPos = -1;

    bool isDark = m_browser->palette().color(QPalette::Base).lightnessF() < 0.5;
    QString hrColor = isDark ? "#444444" : "#cccccc";

    QString promptHtml = applySearchHighlight(markdownToHtml(m_currentPair.prompt.text));
    QString headerHtml = "<h3>Prompt</h3><div style='padding:8px; margin:4px 0;'>" + promptHtml + "</div>";

    QTextCursor cursor(m_browser->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml(headerHtml);

    // --- Attachment Insertion with Position Tracking ---
    QString docId = m_currentPair.prompt.driveDocumentId;
    if (!docId.isEmpty() && m_showAttachments)
    {
        // 1. Record exact start position in document
        cursor.movePosition(QTextCursor::End);
        m_attachmentStartPos = m_browser->document()->characterCount() - 1;

        if (m_attachmentCache.contains(docId))
        {
            auto att = m_attachmentCache[docId];
            cursor.insertHtml(formatAttachmentHtml(att.first, att.second, docId));
        }
        else
        {
            QString boxBg = isDark ? "#2a2a2a" : "#f4f4f4";
            QString boxColor = isDark ? "#888888" : "#666666";
            cursor.insertHtml(QString("<div style='background:%1; color:%2; border-radius:4px; padding:8px; margin:6px 0; font-style:italic;'>"
                                      "&#x2935; [Downloading attachment %3...]</div>")
                                  .arg(boxBg, boxColor, docId.toHtmlEscaped()));
        }

        // 2. Move cursor to true end of document and record end position
        cursor.movePosition(QTextCursor::End);
        m_attachmentEndPos = m_browser->document()->characterCount() - 1;
    }
    else if (!docId.isEmpty() && !m_showAttachments)
    {
        QString badgeBg = isDark ? "#2a2a2a" : "#f1f3f5";
        QString badgeBorder = isDark ? "#3c3c3c" : "#d0d7de";
        QString badgeColor = isDark ? "#888888" : "#666666";
        cursor.insertHtml(QString("<div style='background:%1; color:%2; border:1px dashed %3; border-radius:4px; padding:6px 10px; margin:6px 0; font-size:11px;'>"
                                  "&#x1F4CE; <i>[Attached file hidden — check 'Show Attachments' to view]</i></div>")
                              .arg(badgeBg, badgeColor, badgeBorder));
    }

    // --- Inline Images ---
    if (m_showAttachments)
    {
        if (!m_currentPair.prompt.inlineImages.isEmpty())
        {
            for (int i = 0; i < m_currentPair.prompt.inlineImages.size(); ++i)
            {
                const auto &img = m_currentPair.prompt.inlineImages[i];
                appendInlineImage(img.data, img.mimeType, i);
            }
        }
        else if (!m_currentPair.prompt.inlineImageData.isEmpty())
        {
            appendInlineImage(m_currentPair.prompt.inlineImageData, m_currentPair.prompt.inlineImageMime, 0);
        }
    }

    // Divider & Model Responses
    cursor.movePosition(QTextCursor::End);
    cursor.insertHtml("<hr style='border:none; border-top:1px solid " + hrColor + "; margin:12px 0;'>");

    for (const auto &resp : m_currentPair.responses)
    {
        appendModelResponse(resp.text, resp.isThought, m_showThoughts);
    }

    if (m_currentPair.responses.isEmpty())
        m_browser->append("<p><i>No response yet.</i></p>");

    m_browser->verticalScrollBar()->setValue(0);
}

void ResponsePane::requestAttachment(const QString &fileId, DriveApi *drive)
{
    if (!drive)
        return;

    m_pendingDownloads.insert(fileId);

    if (m_attachmentConn)
        disconnect(m_attachmentConn);

    m_attachmentConn = connect(drive, &DriveApi::attachmentDownloaded, this,
                               [this](const QString &id, const QByteArray &data, const QString &mimeType)
                               {
                                   m_pendingDownloads.remove(id);
                                   m_attachmentCache[id] = qMakePair(data, mimeType);

                                   // Re-render immediately so the attachment appears inside the prompt
                                   if (m_currentPair.prompt.driveDocumentId == id)
                                   {
                                       render();
                                   }
                               });

    drive->downloadAttachment(fileId);
}

QString ResponsePane::formatAttachmentHtml(const QByteArray &data, const QString &mimeType, const QString &fileId)
{
    bool isDark = m_browser->palette().color(QPalette::Base).lightnessF() < 0.5;

    // 1. Image preview
    if (mimeType.startsWith("image/"))
    {
        QPixmap pix;
        if (pix.loadFromData(data))
        {
            QString url = QString("attachment://%1").arg(fileId);
            m_browser->document()->addResource(QTextDocument::ImageResource, QUrl(url), fitImageWidth(pix));
            return QString("<div style='text-align:center; margin:8px 0;'><a href='%1'><img src='%1'/></a></div>").arg(url);
        }
        else
        {
            return "<p><i>[Attached image could not be decoded]</i></p>";
        }
    }

    // 2. Text / Code preview (like codetree.txt, README.md)
    bool isText = mimeType.startsWith("text/") ||
                  mimeType.contains("json") ||
                  mimeType.contains("javascript") ||
                  mimeType.contains("xml") ||
                  !data.contains('\0');

    if (isText)
    {
        QString codeBg = isDark ? "#1e1e1e" : "#f6f8fa";
        QString codeBorder = isDark ? "#3c3c3c" : "#d0d7de";
        QString codeColor = isDark ? "#d4d4d4" : "#24292f";
        QString headerColor = isDark ? "#858585" : "#57606a";

        QString textContent = QString::fromUtf8(data);
        QString highlighted = applySearchHighlight(textContent.toHtmlEscaped());

        return QString("<div style='margin:8px 0;'>"
                       "<div style='font-size:11px; font-weight:bold; margin-bottom:4px;'>"
                       "<a href='attachment://%1' style='text-decoration:none; color:%2;'>&#x1F4CE; Attached File (click or right-click to copy):</a>"
                       "</div>"
                       "<pre style='background:%3; color:%4; border:1px solid %5; border-radius:4px; padding:10px; font-family:Consolas, Monaco, monospace; font-size:12px; white-space:pre-wrap;'>%6</pre>"
                       "</div>")
            .arg(fileId, headerColor, codeBg, codeColor, codeBorder, highlighted);
    }

    // 3. Binary file fallback
    QString binBg = isDark ? "#252526" : "#f0f0f0";
    QString binColor = isDark ? "#aaaaaa" : "#555555";
    return QString("<div style='background:%1; color:%2; border-radius:4px; padding:8px; margin:6px 0;'>"
                   "&#x1F4CE; <a href='attachment://%3' style='text-decoration:none; color:%2;'><i>Attachment downloaded (%4, %5 bytes) - binary format.</i></a></div>")
        .arg(binBg, binColor, fileId, mimeType.toHtmlEscaped()).arg(data.size());
}

void ResponsePane::appendModelResponse(const QString &text, bool isThought, bool showThoughts)
{
    if (isThought && !showThoughts)
        return;

    QString html;
    QString processedText = applySearchHighlight(markdownToHtml(text));
    bool isDark = m_browser->palette().color(QPalette::Base).lightnessF() < 0.5;

    if (text.startsWith("ERROR:", Qt::CaseInsensitive) || text.startsWith("Error:", Qt::CaseInsensitive))
    {
        QString errBg = isDark ? "#4a3b00" : "#fff3cd";
        QString errBorder = isDark ? "#ffc107" : "#ffc107";
        QString errColor = isDark ? "#ffe082" : "#856404";
        html = QString("<div style='background:%1; color:%2; border:1px solid %3; border-radius:4px; padding:8px; margin:4px 0;'>"
                       "<b>&#x26A0; Error / Unsupported Content:</b><br>%4</div>")
                   .arg(errBg, errColor, errBorder, processedText);
    }
    else if (isThought)
    {
        QString thoughtBg     = isDark ? "#252526" : "#f0f0f0";
        QString thoughtColor  = isDark ? "#d4d4d4" : "#333333";
        QString thoughtBorder = isDark ? "#0078d4" : "#888888";
        QString labelColor    = isDark ? "#858585" : "#666666";

        html = QString("<div style='background:%1; color:%2; border-left:3px solid %3; border-radius:4px; padding:8px 12px; margin:6px 0;'>"
                       "<div style='font-style:italic; font-size:11px; color:%4; margin-bottom:4px;'>Thought:</div>"
                       "<div style='color:%2;'>%5</div></div>")
                   .arg(thoughtBg, thoughtColor, thoughtBorder, labelColor, processedText);
    }
    else
    {
        QString textColor = isDark ? "#e0e0e0" : "#1e1e1e";
        html = QString("<div style='color:%1; padding:8px; margin:4px 0;'>%2</div>")
                   .arg(textColor, processedText);
    }

    m_browser->append(html);
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

bool ResponsePane::isPosOverAttachment(const QPoint &pos, QString &outFileId, QString &outImageName) const
{
    outFileId.clear();
    outImageName.clear();

    QString anchor = m_browser->anchorAt(pos);
    QTextCursor cursor = m_browser->cursorForPosition(pos);
    int p = cursor.position();

    qDebug() << "[HitTest] anchorAt:" << anchor
             << "| cursor.position:" << p
             << "| attachmentRange:[" << m_attachmentStartPos << "," << m_attachmentEndPos << "]"
             << "| current driveDocId:" << m_currentPair.prompt.driveDocumentId;

    // Strategy 1: Check anchor
    if (anchor.startsWith("attachment://"))
    {
        outFileId = anchor.mid(13);
        qDebug() << "[HitTest] Matched via anchor:" << outFileId;
        return true;
    }

    // Strategy 2: Check character range
    if (m_attachmentStartPos != -1 && m_attachmentEndPos != -1)
    {
        if (p >= m_attachmentStartPos && p <= m_attachmentEndPos)
        {
            outFileId = m_currentPair.prompt.driveDocumentId;
            qDebug() << "[HitTest] Matched via character range:" << outFileId;
            return true;
        }
    }

    // Strategy 3: Check image format
    QTextCharFormat fmt = cursor.charFormat();
    if (!fmt.isImageFormat())
    {
        QTextCursor prevCursor = cursor;
        if (prevCursor.movePosition(QTextCursor::PreviousCharacter))
            fmt = prevCursor.charFormat();
    }
    if (fmt.isImageFormat())
    {
        outImageName = fmt.toImageFormat().name();
        qDebug() << "[HitTest] Matched via image format:" << outImageName;
        return true;
    }

    qDebug() << "[HitTest] No match found.";
    return false;
}

void ResponsePane::clear()
{
    if (m_attachmentConn)
        disconnect(m_attachmentConn);
    m_attachmentConn = QMetaObject::Connection();
    m_browser->clear();
    m_searchTerm.clear();
    m_currentPair = Conversation::PromptResponsePair();
    m_pendingDownloads.clear();
    m_attachmentStartPos = -1;
    m_attachmentEndPos = -1;
}


void ResponsePane::onCustomContextMenuRequested(const QPoint &pos)
{
    qDebug() << "[ContextMenu] === Right-click detected at viewport pos:" << pos;

    QMenu *menu = m_browser->createStandardContextMenu(pos);
    if (!menu)
        menu = new QMenu(m_browser);

    QString fileId;
    QString imageName;
    bool isOver = isPosOverAttachment(pos, fileId, imageName);

    qDebug() << "[ContextMenu] isPosOverAttachment returned:" << isOver
             << "fileId:" << fileId << "imageName:" << imageName;

    if (isOver)
    {
        QAction *copyAttAction = new QAction(tr("Copy Attachment"), menu);
        connect(copyAttAction, &QAction::triggered, this, [this, fileId, imageName]() {
            qDebug() << "[ContextMenu] 'Copy Attachment' clicked for:" << fileId << imageName;
            copyAttachmentToClipboard(fileId, imageName);
        });

        if (!menu->actions().isEmpty())
        {
            menu->insertAction(menu->actions().first(), copyAttAction);
            menu->insertSeparator(menu->actions().at(1));
        }
        else
        {
            menu->addAction(copyAttAction);
        }
        qDebug() << "[ContextMenu] Successfully inserted 'Copy Attachment' into menu!";
    }

    menu->exec(m_browser->viewport()->mapToGlobal(pos));
    delete menu;
}

void ResponsePane::copyAttachmentToClipboard(const QString &fileId, const QString &imageResourceName)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return;

    // Case 1: Drive attachment loaded in memory cache
    if (!fileId.isEmpty() && m_attachmentCache.contains(fileId))
    {
        const auto &att = m_attachmentCache[fileId];
        const QByteArray &data = att.first;
        const QString &mime = att.second;

        if (mime.startsWith("image/"))
        {
            QPixmap pix;
            if (pix.loadFromData(data))
                clipboard->setPixmap(pix);
        }
        else
        {
            // Text/code file (e.g. codetree.txt, README.md, source code)
            clipboard->setText(QString::fromUtf8(data));
        }
        return;
    }

    // Case 2: Image resource embedded in document (inline image)
    if (!imageResourceName.isEmpty())
    {
        QVariant res = m_browser->document()->resource(QTextDocument::ImageResource, QUrl(imageResourceName));
        if (res.isValid())
        {
            QImage img = res.value<QImage>();
            if (!img.isNull())
            {
                clipboard->setImage(img);
                return;
            }
            QPixmap pix = res.value<QPixmap>();
            if (!pix.isNull())
            {
                clipboard->setPixmap(pix);
                return;
            }
        }
    }
}