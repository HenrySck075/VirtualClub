#ifndef WidgetAnimations_H
#define WidgetAnimations_H

#include <qwidget.h>
namespace anime {
  enum class SlideDirection {
    Left, Right, Up, Down
  };
  void slideFade(QWidget* widget, SlideDirection direction);
};

#endif
