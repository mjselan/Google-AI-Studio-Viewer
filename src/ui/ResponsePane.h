#pragma once

#include <QWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QList>
#include "models/Conversation.h"

class DriveApi;
class FilePreviewWidget;

class ResponsePane : public QWidget
{
    Q_OBJECT
public:
    explicit ResponsePane(QWidget *parent = nullptr);

    void showResponse(const Conversation::PromptResponsePair &pair,
                      bool showThoughts, DriveApi *drive);
    void clear();

private:
    void removeFilePreviews();
    void appendModelResponse(const QString &text, bool isThought, bool showThoughts);
    void appendFileAttachment(const QString &fileId, DriveApi *drive);
    void appendInlineImage(const QString &base64data, const QString &mimeType);

    QTextBrowser *m_browser;
    QVBoxLayout *m_layout;
    QList<FilePreviewWidget*> m_filePreviews;
};
