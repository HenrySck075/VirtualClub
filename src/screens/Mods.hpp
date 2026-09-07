#ifndef SCREENS_Mods_H
#define SCREENS_Mods_H

#include "../utils/ModIndex.hpp"
#include <QWidget>
#include <QStackedWidget>
#include "../utils/FlowLayout.hpp"
#include "ModInfo.hpp"
class DesktopIconWidget;
class ModsScreen : public QWidget {
  Q_OBJECT
  QWidget* m_modsListPage = nullptr;
  QWidget* m_listContent = nullptr;
  FlowLayout* m_contentLayout = nullptr;

  ModInfoScreen* m_modInfoPage = nullptr;

  QStackedWidget* m_contentWrapper = nullptr;
public:
  explicit ModsScreen(QWidget *parent = nullptr);
  void onAddModButtonClicked();
private:
  void addModItem(ModsIndex::Mod& mod);
  void deselectOtherItems(DesktopIconWidget* selectedItem);
  void openModInfo();
};

#endif
