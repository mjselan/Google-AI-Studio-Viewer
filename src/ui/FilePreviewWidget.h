#pragma once

#include <QWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QPushButton>
#include <QByteArray>
#include <QPixmap>

class FilePreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FilePreviewWidget(const QString &fileId, QWidget *parent = nullptr);

    void setContent(const QByteArray &data, const QString &mimeType);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void showImageFullSize();

    QString m_fileId;
    QString m_mimeType;
    QStackedWidget *m_stack;
    QPlainTextEdit *m_textView;
    QLabel *m_imageView;
    QLabel *m_nameLabel;
    QPushButton *m_downloadBtn;
    QPushButton *m_viewBtn;
    QPushButton *m_zoomInBtn;
    QPushButton *m_zoomOutBtn;
    QPushButton *m_fitBtn;
    QWidget *m_imageToolbar;
    QPixmap m_pixmap;
    double m_zoomScale = 1.0;
};
