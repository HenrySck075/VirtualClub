#ifndef MVC_EventFilters_H
#define MVC_EventFilters_H

#include <QEvent>
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

#endif
