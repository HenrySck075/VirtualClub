#ifndef SCREENS_Mods_H
#define SCREENS_Mods_H

#include "../utils/ModIndex.hpp"
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include "../utils/FlowLayout.hpp"
#include "ModInfo.hpp"
#include <QSoundEffect>

class DesktopIconWidget;
class ModsScreen : public QWidget {
  Q_OBJECT
  QWidget* m_modsListPage = nullptr;
  QWidget* m_listContent = nullptr;
  FlowLayout* m_contentLayout = nullptr;

  /// Later on I did thought about making a map of ModInfoScreens instead 
  /// However I decided against that to minimize the memory footprint of the launcher as much as possible.
  ModInfoScreen* m_modInfoPage = nullptr;

  QStackedWidget* m_contentWrapper = nullptr;

  QLabel* m_modsCountLabel = nullptr;
public:
  explicit ModsScreen(QWidget *parent = nullptr);
  void onAddModButtonClicked();
private:
  void addModItem(ModsIndex::Mod mod);
  void removeModItem(std::string modId);
  void deselectOtherItems(DesktopIconWidget* selectedItem);
  void openModInfo(const ModsIndex::Mod& mod);

  void updateModsCountLabel();
};

#endif
