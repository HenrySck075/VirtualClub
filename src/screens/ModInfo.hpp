#ifndef SCREENS_ModInfo_H
#define SCREENS_ModInfo_H

#include <QWidget>
#include <QLabel>
#include "../utils/ModIndex.hpp"
#include "../ui/PixmapWidget.hpp"
class ModInfoScreen : public QWidget {
  Q_OBJECT
public:
  explicit ModInfoScreen(QWidget *parent = nullptr);

  void setDisplayingMod(ModsIndex::Mod& mod);
signals:
  void backButtonClicked();
private:
  PixmapWidget* m_modIconLabel;
  QLabel* m_modNameLabel;
  QLabel* m_modVersionLabel;
};

#endif
