#ifndef SCREENS_ModInfo_H
#define SCREENS_ModInfo_H

#include <QWidget>
#include <QLabel>
#include <optional>
#include "../utils/ModIndex.hpp"
#include "../ui/PixmapWidget.hpp"
#include "ui/Dialog.hpp"
class ModInfoScreen : public QWidget {
  Q_OBJECT
public:
  explicit ModInfoScreen(QWidget *parent = nullptr);

  void setDisplayingMod(ModsIndex::Mod& mod);
  void onPlayButtonClicked();
signals:
  void backButtonClicked();
private:
  PixmapWidget* m_modIconLabel;
  QLabel* m_modNameLabel;
  QLabel* m_modVersionLabel;
  QLabel* m_modDirLabel;

  Button* m_playButton;

  std::optional<ModsIndex::Mod> m_displayingMod;
};

#endif
