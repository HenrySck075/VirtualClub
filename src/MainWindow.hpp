#pragma once
#include "ui/Sidebar.hpp"
#include <QWidget>
#include <QStackedWidget>

class MainWindow : public QWidget {
  Q_OBJECT

  static constexpr int BOTTOM_BAR_HEIGHT = 21;

  QWidget* m_rootWidget;
  QWidget* m_mainContent;
  QStackedWidget* m_stackedWidget;
  Sidebar* m_sidebar; // idk what i should use this for.
  
  std::unordered_map<SidebarItem*, QWidget*> m_navigationMap;
  
  struct {
    std::string currentBackground = "tokyo"; // for now
  } m_settings;
  QImage m_currentBackgroundImage;
public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() {
    m_rootWidget->deleteLater();
  }

protected:
  void initUI();

  // This event handler is automatically called whenever Qt needs to redraw the
  // window
  void paintEvent(QPaintEvent *event) override;

  void onSelectedItemChanged(SidebarItem* oldItem, SidebarItem* newItem);

  SidebarItem* addNavigationItem(QIcon icon, std::string name, SidebarPosition position, QWidget* widget);
};

MainWindow *getMainWindow();
