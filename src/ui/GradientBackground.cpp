#include "GradientBackground.hpp"
#include <QPainter>

void GradientBackground::paintEvent(QPaintEvent* event) {
    QPainter painter(this);

    // the vertical gradient that spans the remaining background
    QLinearGradient bgGradient(0, 0, 0, height());
    bgGradient.setColorAt(0, QColor("#fff7e7"));
    //bgGradient.setColorAt(0, QColor("#000000"));
    bgGradient.setColorAt(1, QColor("#ffffff"));

    painter.fillRect(rect(), bgGradient);
}
