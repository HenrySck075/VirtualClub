#include "IconButton.hpp"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QStyleOptionButton>

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


void IconButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_selectable) setSelected(true);
    }
    
    // Pass the event to the base class if needed
    QPushButton::mouseReleaseEvent(event);
}
IconButton::IconButton(QIcon icon, QWidget *parent) : QPushButton(parent) {
  setFixedSize(40,32);
  // Setup animation (duration: 200 ms)
  m_fadeAnimation = new QPropertyAnimation(this, "hoverAlpha", this);
  //m_fadeAnimation->setDuration(200);
  m_fadeAnimation->setDuration(0);
  m_fadeAnimation->setEasingCurve(QEasingCurve::InOutQuad);

  m_icon = icon;

  setCursor(Qt::PointingHandCursor);
}

IconButton::IconButton(LucideIcon icon, QWidget *parent)
    : IconButton(icon.icon(), parent) {
  m_lucideIcon = std::move(icon);
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
    static const int radius = 4;
    painter.setRenderHint(QPainter::Antialiasing);

    bool enabled = isEnabled();

    // Only paint the background when visible and enabled
    if (enabled && (m_selected || m_hoverAnimationValue > 0)) {
        QPainterPath path;
        path.addRoundedRect(rect(), radius+1, radius+1);
        painter.fillPath(path, QBrush(sm_iconHoverColor));
    }

    // Determine icon color based on enabled state, selection, and animation
    QColor iconColor = sm_iconColor;
    if (enabled) {
        if (!m_selected) {
          iconColor.setRedF(iconColor.redF() + (1.0 - iconColor.redF()) * (m_hoverAnimationValue / 255.0));
          iconColor.setGreenF(iconColor.greenF() + (1.0 - iconColor.greenF()) * (m_hoverAnimationValue / 255.0));
          iconColor.setBlueF(iconColor.blueF() + (1.0 - iconColor.blueF()) * (m_hoverAnimationValue / 255.0));
        } else {
          iconColor = QColorConstants::White;
        }
    } else {
        // Convert to grayscale for disabled state
        int gray = qGray(iconColor.rgb());
        iconColor = QColor(gray, gray, gray, iconColor.alpha());
    }

    // Draw the icon
    if (!m_icon.isNull()) {
        static const int iconSize = 18;
        QPixmap pixmap = m_lucideIcon
            ? m_lucideIcon->pixmap(QSize(iconSize, iconSize), iconColor)
            : m_icon.pixmap(iconSize, iconSize); // Adjust size
        QPixmap coloredPixmap(pixmap.size());
        coloredPixmap.fill(Qt::transparent);
        if (m_lucideIcon) {
          coloredPixmap = pixmap;
        } else {
          QPainter iconPainter(&coloredPixmap);
          iconPainter.setCompositionMode(QPainter::CompositionMode_Source);
          iconPainter.drawPixmap(0, 0, pixmap);
          iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
          iconPainter.fillRect(coloredPixmap.rect(), iconColor);
          iconPainter.end();
        }
        painter.drawPixmap((width() - iconSize) / 2, (height() - iconSize) / 2, coloredPixmap); // Adjust position
    }

    // Draw a rounded border (use a muted grayscale color when disabled)
    QColor borderColor = enabled ? (m_hoverAnimationValue > 0 ? sm_iconHoverColor : sm_borderColor) 
                                 : QColor(180, 180, 180, 120);
    QPen pen(borderColor);
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
