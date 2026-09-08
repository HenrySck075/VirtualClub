#include "IconButton.hpp"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolTip>

void IconButton::setHoverAlpha(int alpha) {
  if (m_hoverAnimationValue != alpha) {
    m_hoverAnimationValue = alpha;
    update(); // Triggers paintEvent() on every animation frame
  }
}
void IconButton::setSelected(bool selected) {
  if (m_selected != selected) {
    m_selected = selected;
    update();
    emit selectedChanged();
  }
}


void IconButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_selectable) setSelected(true);
        emit clicked(); // Emit your signal when left-clicked
    }
    
    // Pass the event to the base class if needed
    QWidget::mousePressEvent(event);
}
IconButton::IconButton(QIcon icon, QWidget *parent) : QWidget(parent) {
  setFixedSize({40,30}); // Set a fixed height for each sidebar item
  // Setup animation (duration: 200 ms)
  m_fadeAnimation = new QPropertyAnimation(this, "hoverAlpha", this);
  m_fadeAnimation->setDuration(200);
  m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);

  m_icon = icon;

  setCursor(Qt::PointingHandCursor);
}
void IconButton::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event);
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_hoverAnimationValue);
    m_fadeAnimation->setEndValue(255); // Max hover opacity (0-255 scale; 100 is ~40%)
    m_fadeAnimation->start();
}

void IconButton::leaveEvent(QEvent *event) {
    Q_UNUSED(event);
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_hoverAnimationValue);
    m_fadeAnimation->setEndValue(0); // Fade back to fully transparent
    m_fadeAnimation->start();
}

void IconButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    static const int radius = 5;
    painter.setRenderHint(QPainter::Antialiasing);

    // Only paint the background when visible
    if (m_selected || m_hoverAnimationValue > 0) {
        // Horizontal gradient from left to right across the widget's rect
        QLinearGradient gradient(rect().topLeft(), rect().topRight());

        // Target color (e.g., White fading out) modulated by m_hoverAlpha

        QColor startColor = sm_hoverColorL; // also icon color 
        QColor stopColor = sm_hoverColorR; // also text color
        if (!m_selected) {
          startColor.setAlpha(m_hoverAnimationValue);
          stopColor.setAlpha(m_hoverAnimationValue);
        }

        gradient.setColorAt(0.0, startColor);
        gradient.setColorAt(1.0, stopColor);
        
        QPainterPath path;
        path.addRoundedRect(rect(), radius+1, radius+1);
        painter.fillPath(path, gradient);
    }

    // icons and the text. color is the interpolation between its default color and white with the value of the hover animation
    QColor iconColor = sm_iconColor;
    if (!m_selected) {
      iconColor.setRedF(iconColor.redF() + (1.0 - iconColor.redF()) * (m_hoverAnimationValue / 255.0));
      iconColor.setGreenF(iconColor.greenF() + (1.0 - iconColor.greenF()) * (m_hoverAnimationValue / 255.0));
      iconColor.setBlueF(iconColor.blueF() + (1.0 - iconColor.blueF()) * (m_hoverAnimationValue / 255.0));
    } else {
      iconColor = QColorConstants::White;
    }
    // Draw the icon
    if (!m_icon.isNull()) {
        static const int iconSize = 20;
        QPixmap pixmap = m_icon.pixmap(iconSize, iconSize); // Adjust size
        QPixmap coloredPixmap(pixmap.size());
        coloredPixmap.fill(Qt::transparent);
        QPainter iconPainter(&coloredPixmap);
        iconPainter.setCompositionMode(QPainter::CompositionMode_Source);
        iconPainter.drawPixmap(0, 0, pixmap);
        iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        iconPainter.fillRect(coloredPixmap.rect(), iconColor);
        iconPainter.end();
        painter.drawPixmap((width() - iconSize) / 2, (height() - iconSize) / 2, coloredPixmap); // Adjust position
    }

    // Draw a rounded border
    QPen pen(sm_borderColor);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), radius, radius); // Adjust for pen width
}

bool IconButton::event(QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    auto *helpEvent = static_cast<QHelpEvent *>(event);

    if (!toolTip().isEmpty()) {
      // Initialize style option as if this were a QPushButton
      QStyleOptionButton opt;
      opt.initFrom(this);

      // Show the tooltip at the mouse position
      QToolTip::showText(helpEvent->globalPos(), toolTip(), this);

      // Return true to indicate the tooltip event was handled
      return true;
    }
  }
  return QWidget::event(event);
}
