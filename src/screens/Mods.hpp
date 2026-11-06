#ifndef SCREENS_Mods_H
#define SCREENS_Mods_H

#include "../utils/ModIndex.hpp"
#include "../ui/Router.hpp"
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include "../utils/FlowLayout.hpp"
#include <QSoundEffect>

class DesktopIconWidget;
class ModsScreen : public QWidget {
  Q_OBJECT
  QWidget* m_modsListPage = nullptr;
  QWidget* m_listContent = nullptr;
  QWidget* m_listContentPlaceholder = nullptr;
  QStackedWidget* m_listContentStack = nullptr;
  FlowLayout* m_contentLayout = nullptr;

  QStackedWidget* m_headerContentStack = nullptr;
  QWidget* m_headerContent = nullptr;
  QWidget* m_headerContentMultiselect = nullptr;
  QLabel* m_modsCountLabel = nullptr;

  Router* m_router = nullptr;

public:
  explicit ModsScreen(Router* router, QWidget *parent = nullptr);
  void onAddModButtonClicked();
private:
  void addModItem(ModsIndex::Mod mod);
  void removeModItem(std::string modId);
  void deselectOtherItems(DesktopIconWidget* selectedItem);
  void openModInfo(const ModsIndex::Mod& mod);

  void updateModsListDisplay();

  void updateModsCountLabel();

  void enableMultiselectToolbar(bool enable);
  void onItemSelected(bool s);
};

#endif
