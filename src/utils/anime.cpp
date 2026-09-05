#include "anime.hpp"
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>

namespace anime {
  void slideFade(QWidget* widget, SlideDirection direction) {
    static const float duration = 400;
    static const QEasingCurve curve = QEasingCurve::OutQuart;


    // code adapted from https://github.com/Qt-Widgets/SlidingStackedWidget-1/blob/master/SlidingStackedWidget/slidingstackedwidget.cpp

    int offset = 30;
    int offsetx, offsety;
    switch (direction) {
      case SlideDirection::Left:
        offsetx = -offset;
        offsety = 0;
        break;
      case SlideDirection::Right:
        offsetx = offset;
        offsety = 0;
        break;
      case SlideDirection::Up:
        offsetx = 0;
        offsety = offset;
        break;
      case SlideDirection::Down:
        offsetx = 0;
        offsety = -offset;
        break;
    }
    widget->move(offsetx,offsety);

    auto* moveAnim = new QPropertyAnimation(widget, "pos");
    moveAnim->setDuration(duration);
    moveAnim->setEasingCurve(curve);
    moveAnim->setStartValue(QPoint(offsetx,offsety));
    moveAnim->setEndValue(QPoint(0,0));

    auto* fadeAnimE = new QGraphicsOpacityEffect();
    widget->setGraphicsEffect(fadeAnimE);
    auto* fadeAnim = new QPropertyAnimation(fadeAnimE, "opacity");
    fadeAnim->setDuration(duration/3*2);
    fadeAnim->setEasingCurve(curve);
    fadeAnim->setStartValue(0);
    fadeAnim->setEndValue(1);
    fadeAnim->connect(fadeAnim, &QPropertyAnimation::finished, [=](){fadeAnimE->deleteLater();});

    auto* animgroup = new QParallelAnimationGroup;
    animgroup->addAnimation(moveAnim);
    animgroup->addAnimation(fadeAnim);
    animgroup->start(QAbstractAnimation::DeleteWhenStopped);

  }
}
