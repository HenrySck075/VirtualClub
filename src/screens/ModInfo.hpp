#ifndef SCREENS_ModInfo_H
#define SCREENS_ModInfo_H

#include <QWidget>
#include <QLabel>
#include <optional>
#include "utils/ModIndex.hpp"
#include "ui/PixmapWidget.hpp"
#include "ui/MESWidgets.hpp"
#include "ui/Router.hpp"

class ModInfoScreen;

namespace detail {
class SSDCItem : public QWidget {
  Q_OBJECT
signals:
  void clicked();
private:
  QPointer<PixmapWidget> m_thumbnail;
  QString m_name;
  QString m_dateTimeStr;

  float m_hoverAnimationTimer = 0.0;
  QVariantAnimation* m_hoverAnimation;
public:
  explicit SSDCItem(PixmapWidget *thumbnail, const QString &name,
                    const QString &dateTimeStr, QWidget *parent = nullptr);

protected:
  void mouseReleaseEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

  // only need to paint a pink background.
  // except this rectangle is 8px bigger than the widget
  void paintEvent(QPaintEvent *event) override;
};

class SaveSelectDialogContent : public DialogContent {
  Q_OBJECT
  ModsIndex::Mod m_mod;

  QPointer<::ModInfoScreen> m_modInfoScreen;
public:
  // Get the folder where Ren'Py store every game's save data.
  static std::filesystem::path getRenpySaveDirectory();
  explicit SaveSelectDialogContent(const ModsIndex::Mod &mod,
                                   ModInfoScreen *mic);
};
}


class ModInfoScreen : public QWidget {
  Q_OBJECT
public:
  friend class detail::SaveSelectDialogContent;
  explicit ModInfoScreen(Router* router, QWidget *parent = nullptr);

  void setDisplayingMod(const ModsIndex::Mod& mod);
signals:
  void backButtonClicked();
  void modUninstalled(std::string modId);
private:
  void onPlayButtonClicked();
  void play(const QString& saveId = "");
  void onOpenModDirClicked();
  void onDeleteButtonClicked();
  void updatePlaytimeLabel();

  void setMutationWidgetsEnabled(bool enabled);

  PixmapWidget* m_modIconLabel = nullptr;
  QLabel* m_modNameLabel = nullptr;
  QLabel* m_modVersionLabel = nullptr;
  QLabel* m_playtimeLabel = nullptr;

  Button* m_playButton = nullptr;
  Button* m_playFromSaveButton = nullptr;
  Button* m_deleteButton = nullptr;

  Switch* m_developerModeSwitch = nullptr;
  Switch* m_forceRecompileSwitch = nullptr;

  Router* m_router = nullptr;

  std::optional<ModsIndex::Mod> m_displayingMod;

  friend class SaveSelectDialogContent;
};

#endif
