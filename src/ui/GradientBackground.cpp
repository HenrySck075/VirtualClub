#include "GradientBackground.hpp"
#include <QPainter>
#include <QEvent>

void GradientBackground::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    // the vertical gradient that spans the remaining background
    QLinearGradient bgGradient(0, 0, 0, height());
    bgGradient.setColorAt(0, QColor("#fff7e7"));
    //bgGradient.setColorAt(0, QColor("#000000"));
    bgGradient.setColorAt(1, QColor("#ffffff"));

    painter.fillRect(rect(), bgGradient);
}
bool GradientBackgroundFilter::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Paint) {
    if (QWidget *w = qobject_cast<QWidget *>(obj)) {
      QPainter painter(w);
      QLinearGradient gradient(0, 0, 0, w->height());
      gradient.setColorAt(0, QColor("#fff7e7"));
      gradient.setColorAt(1, QColor("#ffffff"));
      painter.fillRect(w->rect(), gradient);
    }
  }
  return QObject::eventFilter(obj, event); 
}
