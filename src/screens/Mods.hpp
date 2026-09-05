#ifndef SCREENS_Mods_H
#define SCREENS_Mods_H

#include "../utils/ModIndex.hpp"
#include <QWidget>
#include <QGridLayout>
class ModsScreen : public QWidget {
  Q_OBJECT
  QWidget* m_content = nullptr;
  QGridLayout* m_contentLayout = nullptr;
public:
  explicit ModsScreen(QWidget *parent = nullptr);
  void onAddModButtonClicked();
private:
  void addModItem(ModsIndex::Mod& mod);
};

#endif
