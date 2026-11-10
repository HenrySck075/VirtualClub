#pragma once
#include "ui/Sidebar.hpp"
#include "ui/Router.hpp"
#include <QWidget>
#include <QStackedWidget>
#include <QMainWindow>
#include <QSystemTrayIcon>

class MainWindow : public QWidget {
  Q_OBJECT

  static constexpr int BOTTOM_BAR_HEIGHT = 21;

  QWidget* m_rootWidget;
  QWidget* m_mainContent;
  Router* m_router;
  Sidebar* m_sidebar; // idk what i should use this for.
  
  std::unordered_map<SidebarItem*, QString> m_navigationMap;
  std::list<SidebarItem*> m_actionMap;

  QSystemTrayIcon* m_trayIcon = nullptr;

  QString m_currentPath = "/home";
  
  struct {
    std::string currentBackground = "tokyo"; // for now
  } m_settings;
  QImage m_currentBackgroundImage;
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() {
    m_rootWidget->deleteLater();
  }

  void setPageTitleBar(const QString& pageTitle);
  void updateTrayTooltip();
protected:
  void initUI();

  // This event handler is automatically called whenever Qt needs to redraw the
  // window
  void paintEvent(QPaintEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

  void onSelectedItemChanged(SidebarItem* oldItem, SidebarItem* newItem);
  void routeTransitionHandler(QString path, QWidget* outgoing, QWidget* incoming, std::function<void()> onFinished);
  SidebarItem* sidebarItemForPath(QString& path);

  SidebarItem* addNavigationItem(QIcon icon, QString name, SidebarPosition position, QString route, QWidget* widget);
  SidebarItem* addActionItem(QIcon icon, QString name, SidebarPosition position, std::function<void()> onClicked);

  void setupTrayIcon();
  bool confirmQuit();
};

MainWindow *getMainWindow();
