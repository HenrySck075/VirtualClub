/// TODO: change this to another loading circle graphic and move to MESWidgets.hpp
#pragma once

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPaintEvent>
#include <QShowEvent>
#include <QHideEvent>
#include "consts.hpp"
#include "utils/LucideIcons.hpp"

/// literally LucideIcons::loader_circle spinning clockwise non-stop
class LoadingCircle : public QWidget {
  Q_OBJECT
private:
  QTimer m_timer;
  qreal m_angle = 0.0;
  qreal m_scale = 1.0;

  QColor m_color = c_primaryColor;
public:
  explicit LoadingCircle(QWidget *parent = nullptr);

  void setColor(const QColor &color);
  void setScale(qreal scale);

protected:
  void showEvent(QShowEvent *event) override;

  void hideEvent(QHideEvent *event) override;

  void paintEvent(QPaintEvent *) override;

  QSize sizeHint() const override;
};
