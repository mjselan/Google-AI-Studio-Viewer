#pragma once

#include <QDialog>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QSlider>
#include <QPixmap>

class ImageViewerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ImageViewerDialog(const QPixmap &pixmap, const QString &title,
                               QWidget *parent = nullptr);

    static bool viewImage(const QPixmap &pixmap, const QString &title,
                          QWidget *parent = nullptr);

private slots:
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void zoomOriginal();
    void saveImage();

private:
    void updateZoomLabel();

    QPixmap m_original;
    QLabel *m_imageLabel;
    QScrollArea *m_scrollArea;
    QSlider *m_zoomSlider;
    QLabel *m_zoomLabel;
    QPushButton *m_btnZoomIn;
    QPushButton *m_btnZoomOut;
    QPushButton *m_btnFit;
    QPushButton *m_btnOriginal;
    QPushButton *m_btnSave;
    double m_scale = 1.0;
};
