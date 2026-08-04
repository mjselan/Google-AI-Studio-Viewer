#include "FilePreviewWidget.h"
#include "ImageViewerDialog.h"
#include "app/Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QCursor>

FilePreviewWidget::FilePreviewWidget(const QString &fileId, QWidget *parent)
    : QWidget(parent)
    , m_fileId(fileId)
    , m_stack(new QStackedWidget(this))
    , m_textView(new QPlainTextEdit(this))
    , m_imageView(new QLabel(this))
    , m_nameLabel(new QLabel("File: " + fileId, this))
    , m_downloadBtn(new QPushButton("Save As...", this))
    , m_viewBtn(new QPushButton("View Full Size", this))
    , m_zoomInBtn(new QPushButton("+", this))
    , m_zoomOutBtn(new QPushButton("-", this))
    , m_fitBtn(new QPushButton("Fit", this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *header = new QHBoxLayout;
    header->addWidget(m_nameLabel);
    header->addStretch();
    header->addWidget(m_viewBtn);
    header->addWidget(m_downloadBtn);
    layout->addLayout(header);

    // Image toolbar (hidden by default)
    m_imageToolbar = new QWidget(this);
    auto *imageToolbarLayout = new QHBoxLayout(m_imageToolbar);
    imageToolbarLayout->setContentsMargins(0, 0, 0, 0);
    imageToolbarLayout->addWidget(m_zoomOutBtn);
    imageToolbarLayout->addWidget(m_fitBtn);
    imageToolbarLayout->addWidget(m_zoomInBtn);
    imageToolbarLayout->addStretch();
    m_imageToolbar->setVisible(false);
    layout->addWidget(m_imageToolbar);

    m_textView->setReadOnly(true);
    m_textView->setFont(QFont("monospace", 10));
    m_textView->setMaximumHeight(300);

    m_imageView->setAlignment(Qt::AlignCenter);
    m_imageView->setMaximumHeight(400);
    m_imageView->setCursor(Qt::PointingHandCursor);
    m_imageView->setToolTip("Double-click to view full size");

    m_stack->addWidget(m_textView);
    m_stack->addWidget(m_imageView);
    m_stack->setVisible(false);
    layout->addWidget(m_stack, 1);

    connect(m_downloadBtn, &QPushButton::clicked, this, [this]()
    {
        if (m_pixmap.isNull())
            return;
        QString path = QFileDialog::getSaveFileName(this, "Save Image",
            m_fileId + ".png", "Images (*.png *.jpg *.bmp);;All Files (*)");
        if (!path.isEmpty())
        {
            if (!m_pixmap.save(path))
                QMessageBox::warning(this, "Save Error", "Failed to save image.");
        }
    });

    connect(m_viewBtn, &QPushButton::clicked, this, &FilePreviewWidget::showImageFullSize);

    connect(m_zoomInBtn, &QPushButton::clicked, this, [this]()
    {
        m_zoomScale = qMin(m_zoomScale * 1.25, 4.0);
        m_imageView->setPixmap(m_pixmap.scaled(
            m_pixmap.size() * m_zoomScale, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });

    connect(m_zoomOutBtn, &QPushButton::clicked, this, [this]()
    {
        m_zoomScale = qMax(m_zoomScale / 1.25, 0.1);
        m_imageView->setPixmap(m_pixmap.scaled(
            m_pixmap.size() * m_zoomScale, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });

    connect(m_fitBtn, &QPushButton::clicked, this, [this]()
    {
        QSize maxSize(400, 400);
        double scaleX = double(maxSize.width()) / m_pixmap.width();
        double scaleY = double(maxSize.height()) / m_pixmap.height();
        m_zoomScale = qMin(scaleX, scaleY);
        m_imageView->setPixmap(m_pixmap.scaled(
            m_pixmap.size() * m_zoomScale, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
}

void FilePreviewWidget::setContent(const QByteArray &data, const QString &mimeType)
{
    m_mimeType = mimeType;

    Logger::instance().info(
        QString("[Attachment] FilePreviewWidget::setContent: fileId='%1' mime='%2' dataSize=%3")
            .arg(m_fileId, mimeType).arg(data.size()));

    if (mimeType.startsWith("text/") || mimeType.contains("json") || mimeType.contains("javascript"))
    {
        Logger::instance().info(
            QString("[Attachment] FilePreviewWidget: rendering as text for fileId='%1'").arg(m_fileId));
        m_textView->setPlainText(QString::fromUtf8(data));
        m_stack->setCurrentIndex(0);
        m_stack->setVisible(true);
        m_downloadBtn->setVisible(false);
        m_viewBtn->setVisible(false);
    }
    else if (mimeType.startsWith("image/"))
    {
        QPixmap pix;
        if (pix.loadFromData(data))
        {
            Logger::instance().info(
                QString("[Attachment] FilePreviewWidget: image decoded OK for fileId='%1'")
                    .arg(m_fileId));
            m_pixmap = pix;
            m_zoomScale = 1.0;
            m_imageView->setPixmap(pix.scaledToWidth(400, Qt::SmoothTransformation));
            m_stack->setCurrentIndex(1);
            m_stack->setVisible(true);
            m_downloadBtn->setVisible(true);
            m_viewBtn->setVisible(true);
            m_imageToolbar->setVisible(true);
        }
        else
        {
            Logger::instance().warn(
                QString("[Attachment] FilePreviewWidget: IMAGE DECODE FAILED mime='%1' size=%2 "
                        "for fileId='%3'")
                    .arg(mimeType).arg(data.size()).arg(m_fileId));
        }
    }
    else
    {
        Logger::instance().info(
            QString("[Attachment] FilePreviewWidget: unknown mime '%1' for fileId='%2', no preview")
                .arg(mimeType, m_fileId));
        m_stack->setVisible(false);
        m_downloadBtn->setVisible(true);
        m_viewBtn->setVisible(false);
    }
}

void FilePreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_pixmap.isNull())
        showImageFullSize();
    QWidget::mousePressEvent(event);
}

void FilePreviewWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_pixmap.isNull())
        showImageFullSize();
    QWidget::mouseDoubleClickEvent(event);
}

void FilePreviewWidget::showImageFullSize()
{
    if (!m_pixmap.isNull())
        ImageViewerDialog::viewImage(m_pixmap, m_fileId, this);
}
