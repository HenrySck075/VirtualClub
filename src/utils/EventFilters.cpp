#include "EventFilters.hpp"

ChildResizerFilter::ChildResizerFilter(QWidget *child, QObject *parent)
    : QObject(parent), m_child(child) {}
bool ChildResizerFilter::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::Resize && !m_isResizing) {
    auto *parentWidget = qobject_cast<QWidget *>(watched);
    if (parentWidget && m_child) {
      // Keep child sized to match parent
      m_isResizing = true;
      m_child->resize(parentWidget->size());
      m_isResizing = false;
    }
  }
  return QObject::eventFilter(watched, event);
}
bool ClickEventFilter::eventFilter(QObject *watched, QEvent *event) {
  QWidget *widget = qobject_cast<QWidget *>(watched);
  if (!widget) {
    return QObject::eventFilter(watched, event);
  }

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      m_mousePressed = true;
      // Return true here if you want to consume/block the press event
      return true;
    }
    break;
  }
  case QEvent::MouseButtonRelease: {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton && m_mousePressed) {
      m_mousePressed = false;

      // Ensure the release occurred within the target widget's area
      if (widget->rect().contains(mouseEvent->position().toPoint())) {
        emit clicked(widget);
        // Return true if you want to consume the click event
        return false;
      }
    }
    break;
  }
  default:
    break;
  }

  // Pass the event on to the base class / target object
  return QObject::eventFilter(watched, event);
}
