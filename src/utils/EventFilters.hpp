#ifndef MVC_EventFilters_H
#define MVC_EventFilters_H

#include <QEvent>
#include <QMouseEvent>
#include <QWidget>
#include <QPointer>

class ChildResizerFilter : public QObject {
    Q_OBJECT
public:
    ChildResizerFilter(QWidget *child, QObject *parent = nullptr)
        : QObject(parent), m_child(child) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Resize && !m_isResizing) {
            auto *parentWidget = qobject_cast<QWidget*>(watched);
            if (parentWidget && m_child) {
                // Keep child sized to match parent
                m_isResizing = true;
                m_child->resize(parentWidget->size());
                m_isResizing = false;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QPointer<QWidget> m_child;
    bool m_isResizing = false;
};

class ClickEventFilter : public QObject
{
  Q_OBJECT

public:
  explicit ClickEventFilter(QObject *parent = nullptr) : QObject(parent) {}

signals:
  // Signal emitted when a valid click is detected
  void clicked(QWidget *target);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    QWidget *widget = qobject_cast<QWidget*>(watched);
    if (!widget) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
      auto *mouseEvent = static_cast<QMouseEvent*>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
          m_mousePressed = true;
          // Return true here if you want to consume/block the press event
          return false; 
      }
      break;
    }
    case QEvent::MouseButtonRelease: {
      auto *mouseEvent = static_cast<QMouseEvent*>(event);
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

private:
    bool m_mousePressed = false;
};

#endif
