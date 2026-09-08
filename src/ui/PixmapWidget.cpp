#include "PixmapWidget.hpp"
#include <QPainter>

PixmapWidget::PixmapWidget(QWidget *parent) 
    : QWidget(parent) {}

void PixmapWidget::setPixmap(const QPixmap &pixmap) {
    m_pixmap = pixmap;
    update(); // Triggers a paintEvent repaint call
}

void PixmapWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    if (m_pixmap.isNull()) return;

    QPainter painter(this);
    
    // Scale image while preserving aspect ratio inside widget bounds
    QPixmap scaledPixmap = m_pixmap.scaled(size(), 
                                           Qt::KeepAspectRatio, 
                                           Qt::SmoothTransformation);

    // Center the image inside the widget area
    int x = (width() - scaledPixmap.width()) / 2;
    int y = (height() - scaledPixmap.height()) / 2;

    painter.drawPixmap(x, y, scaledPixmap);
}
