#include "LoadingCircle.hpp"
LoadingCircle::LoadingCircle(QWidget *parent) : QWidget(parent) {
  m_timer.setInterval(1 / 60 * 1000);
  connect(&m_timer, &QTimer::timeout, this, [this]() {
    m_angle += 2.0 / (m_scale * (qMin(width(), height()) / 36));
    if (m_angle >= 360.0) {
      m_angle -= 360.0;
    }
    update();
  });
}
void LoadingCircle::setColor(const QColor &color) {
  m_color = color;
  update();
}
void LoadingCircle::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  m_timer.start();
}
void LoadingCircle::hideEvent(QHideEvent *event) {
  QWidget::hideEvent(event);
  m_timer.stop();
}
void LoadingCircle::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setPen(m_color);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  const int side = qMin(width(), height());
  if (side <= 0)
    return;

  painter.translate((width() / 2.0), (height() / 2.0));
  painter.rotate(m_angle);

  QPixmap pixmap = LucideIcons::loader_circle.pixmap(QSize(side, side), m_color);
  painter.drawPixmap(-pixmap.width() / 2, -pixmap.height() / 2, pixmap);
}
QSize LoadingCircle::sizeHint() const { return QSize(24*m_scale, 24*m_scale); }
void LoadingCircle::setScale(qreal scale) {
  m_scale = scale;
  update();
}
