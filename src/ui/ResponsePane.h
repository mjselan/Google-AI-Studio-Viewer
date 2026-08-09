#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QByteArray>
#include <QMetaObject>
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
    void appendModelResponse(const QString &text, bool isThought, bool showThoughts);
    void requestAttachment(const QString &fileId, DriveApi *drive);
    void insertAttachmentInline(const QByteArray &data, const QString &mimeType,
                                const QString &fileId);
    void appendInlineImage(const QString &base64data, const QString &mimeType, int index = 0);
    QString applySearchHighlight(const QString &text) const;

    QTextBrowser *m_browser;
    QVBoxLayout *m_layout;
    QMetaObject::Connection m_attachmentConn;
    QString m_searchTerm;
};