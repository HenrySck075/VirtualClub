#ifndef MVC_EventFilters_H
#define MVC_EventFilters_H

#include <QEvent>
#include <QWidget>

class ChildResizerFilter : public QObject {
    Q_OBJECT
public:
    ChildResizerFilter(QWidget *child, QObject *parent = nullptr)
        : QObject(parent), m_child(child) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::Resize) {
            auto *parentWidget = qobject_cast<QWidget*>(watched);
            if (parentWidget && m_child) {
                // Keep child sized to match parent
                m_child->resize(parentWidget->size());
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QWidget *m_child;
};

#endif
