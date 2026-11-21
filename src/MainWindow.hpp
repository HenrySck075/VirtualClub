#pragma once
#include "ui/Sidebar.hpp"
#include "ui/Router.hpp"
#include <QWidget>
#include <QStackedWidget>
#include <QMainWindow>
#include "utils/LucideIcons.hpp"
#include <nlohmann/json.hpp>

class HomeScreen;

class MainWindow : public QWidget {
  Q_OBJECT

  static constexpr int BOTTOM_BAR_HEIGHT = 21;

  QWidget* m_rootWidget;
  QWidget* m_mainContent;
  Router* m_router;
  Sidebar* m_sidebar; // idk what i should use this for.
  HomeScreen* m_homeScreen = nullptr;
  
  std::unordered_map<SidebarItem*, QString> m_navigationMap;
  std::list<SidebarItem*> m_actionMap;

  QString m_currentPath = "/home";
  
  struct {
    std::string currentBackground = "tokyo"; // for now
  } m_settings;
  QImage m_currentBackgroundImage;

  QSoundEffect m_topLevelNavSfx;
  QSoundEffect m_subLevelNavSfx;
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() {
    m_rootWidget->deleteLater();
  }

  void setPageTitleBar(const QString& pageTitle);
  void setBackgroundPack(const QString& pack);
  void showUpdateAvailable(const nlohmann::json& manifest);
protected:
  void initUI();

  // This event handler is automatically called whenever Qt needs to redraw the
  // window
  void paintEvent(QPaintEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  void onSelectedItemChanged(SidebarItem* oldItem, SidebarItem* newItem);
  void routeTransitionHandler(QString path, QWidget* outgoing, QWidget* incoming, std::function<void()> onFinished);
  SidebarItem* sidebarItemForPath(QString& path);

  SidebarItem* addNavigationItem(LucideIcon icon, QString name, SidebarPosition position, QString route, QWidget* widget);
  SidebarItem* addActionItem(LucideIcon icon, QString name, SidebarPosition position, std::function<void()> onClicked);

  bool confirmQuit();

};

MainWindow *getMainWindow();
