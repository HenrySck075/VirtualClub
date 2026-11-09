#ifndef SCREENS_Mods_H
#define SCREENS_Mods_H

#include "../utils/ModIndex.hpp"
#include "../ui/Router.hpp"
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include "../utils/FlowLayout.hpp"
#include <QSoundEffect>

namespace detail {
class DesktopIconWidget : public QWidget {
    Q_OBJECT

public:
  explicit DesktopIconWidget(const QPixmap &icon, const QString &label,
                             QWidget *parent = nullptr);

  bool isSelected() const;
  void setSelected(bool selected);

  QString label() const;
signals:
  void clicked();
  void doubleClicked();
  void deselectOtherIconsEvent(DesktopIconWidget* icon);
  void multiselectStart();
  void multiselectEnd();
  void selected(bool s);

protected:
  void paintEvent(QPaintEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  QSize sizeHint() const override;

private:
  QPixmap m_icon;
  QString m_label;
  bool m_isHovered = false;
  bool m_isSelected = false;

  // putting this here for now
  const bool m_doubleClickToOpen = true;
};
}

class ModsScreen : public QWidget {
  Q_OBJECT
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
  void deselectOtherItems(detail::DesktopIconWidget* selectedItem);
  void openModInfo(const ModsIndex::Mod& mod);

  void updateModsListDisplay();

  void updateModsCountLabel();

  void enableMultiselectToolbar(bool enable);
  void onItemSelected(bool s);
};

#endif
