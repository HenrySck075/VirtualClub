#ifndef CONSTS_HPP
#define CONSTS_HPP

#include <QColor>
#include <QSoundEffect>

const QColor c_primaryColor(137,35,137);
const QColor c_secondaryColor(233,25,133);
const QColor c_primaryLightColor(197,166,196);
const QColor c_secondaryLightColor(227,162,195);
const QColor c_disabledColor(150,150,150);
inline QSoundEffect* c_navigationSfx = nullptr;

inline void initGlobalSfx() {
  if (!c_navigationSfx) {
    c_navigationSfx = new QSoundEffect();
    c_navigationSfx->setSource(QUrl("qrc:/audio/navigation.wav"));
  }
}

#endif
