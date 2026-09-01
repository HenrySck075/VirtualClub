#include "CoverImageWidget.hpp"

void CoverImageWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  if (m_pixmap.isNull()) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
//  painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);

  // 1. Scale the pixmap using Qt::KeepAspectRatioByExpanding (Cover Mode)
  QPixmap scaledPixmap = m_pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);

  // 2. Calculate offsets to center-crop the scaled pixmap
  int x = (scaledPixmap.width() - width()) / 2;
  int y = (scaledPixmap.height() - height()) / 2;

  // 3. Draw only the visible target region
  painter.drawPixmap(rect(), scaledPixmap, QRect(x, y, width(), height()));
  painter.fillRect(rect(), QColor(0, 0, 0, 0.4*255));
}
