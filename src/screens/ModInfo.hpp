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

  void setDisplayingMod(const ModsIndex::Mod& mod);
signals:
  void backButtonClicked();
  void modUninstalled(std::string modId);
private:
  void onPlayButtonClicked();
  void onOpenModDirClicked();
  void onDeleteButtonClicked();

  PixmapWidget* m_modIconLabel = nullptr;
  QLabel* m_modNameLabel = nullptr;
  QLabel* m_modVersionLabel = nullptr;

  Button* m_playButton = nullptr;
  Button* m_playFromSaveButton = nullptr;
  Button* m_deleteButton = nullptr;

  std::optional<ModsIndex::Mod> m_displayingMod;
};

#endif
