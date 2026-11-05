#ifndef WidgetAnimations_H
#define WidgetAnimations_H

#include <QWidget> 
#include <QAbstractAnimation>
namespace anime {
  enum class SlideDirection {
    Left, Right, Up, Down
  };
  QAbstractAnimation* slideFade(QWidget* widget, SlideDirection direction);
};

#endif
