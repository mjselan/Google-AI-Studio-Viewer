#include "ImageViewerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>

ImageViewerDialog::ImageViewerDialog(const QPixmap &pixmap, const QString &title,
                                     QWidget *parent)
    : QDialog(parent)
    , m_original(pixmap)
    , m_imageLabel(new QLabel(this))
    , m_scrollArea(new QScrollArea(this))
    , m_zoomSlider(new QSlider(Qt::Horizontal, this))
    , m_zoomLabel(new QLabel("100%", this))
    , m_btnZoomIn(new QPushButton("+", this))
    , m_btnZoomOut(new QPushButton("-", this))
    , m_btnFit(new QPushButton("Fit", this))
    , m_btnOriginal(new QPushButton("1:1", this))
    , m_btnSave(new QPushButton("Save", this))
{
    setWindowTitle("Image: " + title);
    resize(900, 700);

    m_imageLabel->setPixmap(m_original);
    m_imageLabel->setAlignment(Qt::AlignCenter);

    m_scrollArea->setWidget(m_imageLabel);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setAlignment(Qt::AlignCenter);

    m_zoomSlider->setRange(10, 400);
    m_zoomSlider->setValue(100);
    m_zoomSlider->setTickPosition(QSlider::TicksBelow);
    m_zoomSlider->setTickInterval(50);

    auto *mainLayout = new QVBoxLayout(this);

    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(m_btnZoomOut);
    toolbar->addWidget(m_zoomSlider);
    toolbar->addWidget(m_btnZoomIn);
    toolbar->addWidget(m_zoomLabel);
    toolbar->addStretch();
    toolbar->addWidget(m_btnFit);
    toolbar->addWidget(m_btnOriginal);
    toolbar->addWidget(m_btnSave);
    mainLayout->addLayout(toolbar);
    mainLayout->addWidget(m_scrollArea, 1);

    connect(m_btnZoomIn, &QPushButton::clicked, this, &ImageViewerDialog::zoomIn);
    connect(m_btnZoomOut, &QPushButton::clicked, this, &ImageViewerDialog::zoomOut);
    connect(m_btnFit, &QPushButton::clicked, this, &ImageViewerDialog::zoomFit);
    connect(m_btnOriginal, &QPushButton::clicked, this, &ImageViewerDialog::zoomOriginal);
    connect(m_btnSave, &QPushButton::clicked, this, &ImageViewerDialog::saveImage);
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value)
    {
        m_scale = value / 100.0;
        m_imageLabel->setPixmap(m_original.scaled(
            m_original.size() * m_scale, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        updateZoomLabel();
    });
}

bool ImageViewerDialog::viewImage(const QPixmap &pixmap, const QString &title,
                                  QWidget *parent)
{
    ImageViewerDialog dlg(pixmap, title, parent);
    return dlg.exec() == Accepted;
}

void ImageViewerDialog::zoomIn()
{
    m_zoomSlider->setValue(m_zoomSlider->value() + 25);
}

void ImageViewerDialog::zoomOut()
{
    m_zoomSlider->setValue(m_zoomSlider->value() - 25);
}

void ImageViewerDialog::zoomFit()
{
    QSize viewport = m_scrollArea->viewport()->size();
    double scaleX = double(viewport.width()) / m_original.width();
    double scaleY = double(viewport.height()) / m_original.height();
    m_scale = qMin(scaleX, scaleY) * 0.9;
    m_zoomSlider->setValue(qRound(m_scale * 100));
}

void ImageViewerDialog::zoomOriginal()
{
    m_zoomSlider->setValue(100);
}

void ImageViewerDialog::saveImage()
{
    QString path = QFileDialog::getSaveFileName(this, "Save Image", "image.png",
        "Images (*.png *.jpg *.bmp)");
    if (!path.isEmpty())
    {
        if (!m_original.save(path))
            QMessageBox::warning(this, "Save Error", "Failed to save image.");
    }
}

void ImageViewerDialog::updateZoomLabel()
{
    m_zoomLabel->setText(QString("%1%").arg(qRound(m_scale * 100)));
}
