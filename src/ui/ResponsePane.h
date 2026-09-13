#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QByteArray>
#include <QMetaObject>
#include <QHash>
#include <QSet>
#include "models/Conversation.h"

class DriveApi;

class ResponsePane : public QWidget
{
    Q_OBJECT
public:
    explicit ResponsePane(QWidget *parent = nullptr);

    void showResponse(const Conversation::PromptResponsePair &pair,
                      bool showThoughts, DriveApi *drive);
    void setSearchTerm(const QString &term);
    void clear();

private:
    void render();
    void appendModelResponse(const QString &text, bool isThought, bool showThoughts);
    void requestAttachment(const QString &fileId, DriveApi *drive);
    QString formatAttachmentHtml(const QByteArray &data, const QString &mimeType, const QString &fileId);
    void appendInlineImage(const QString &base64data, const QString &mimeType, int index = 0);
    QString applySearchHighlight(const QString &html) const;

    QTextBrowser *m_browser;
    QVBoxLayout *m_layout;
    QMetaObject::Connection m_attachmentConn;
    QString m_searchTerm;

    Conversation::PromptResponsePair m_currentPair;
    bool m_showThoughts = false;
    DriveApi *m_drive = nullptr;

    // Cache downloaded attachments in memory (fileId -> {data, mimeType})
    QHash<QString, QPair<QByteArray, QString>> m_attachmentCache;
    QSet<QString> m_pendingDownloads;
};