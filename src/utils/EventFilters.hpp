#ifndef MVC_EventFilters_H
#define MVC_EventFilters_H

#include <QEvent>
#include <QMouseEvent>
#include <QWidget>
#include <QPointer>

class ChildResizerFilter : public QObject {
    Q_OBJECT
public:
  ChildResizerFilter(QWidget *child, QObject *parent = nullptr);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

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
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool m_mousePressed = false;
};

#endif
