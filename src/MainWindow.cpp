#include <QPainter>
#include <QLayout>
#include <QScrollArea>
#include <QEvent>
#include <QCoreApplication>
#include <QApplication>
#include <QTime>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QMenu>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include "MainWindow.hpp"
#include "consts.hpp"
#include "screens/Home.hpp"
#include "screens/Mods.hpp"
#include "screens/Settings.hpp"
#include "screens/SwitchUserDialog.hpp"
#include "ui/MESWidgets.hpp"
#include "ui/Sidebar.hpp"
#include "ui/CoverImageWidget.hpp"
#include "utils/BackgroundLoader.hpp"
#include "utils/LucideIcons.hpp"
#include "utils/ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include "utils/anime.hpp"

QString turkye(const QColor& color) {
  return QString("rgb(%1,%2,%3)")
    .arg(color.red())
    .arg(color.green())
    .arg(color.blue())
  ;
}

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  setPageTitleBar("");
  setMinimumSize(QSize(600,400));
  resize(1020, 600); // Set default starting size (Width, Height)

  //m_currentBackgroundImage = getCurrentImage(m_settings.currentBackground);
  initUI();
  setupTrayIcon();

  setStyleSheet(QString(R"(
QLabel {
  font-family: Quicksand, Segoe UI;
  color: %1;
}

QLineEdit {
  border: 0px;
  border-bottom: 2px solid %2;
  font-family: Quicksand, Segoe UI;
  padding: 4px;
}
)").arg(turkye(c_primaryColor)).arg(turkye(c_secondaryColor)));
}

void MainWindow::setPageTitleBar(const QString& pageTitle) {
  auto title = QString("VirtualClub Ren'Py Mod Manager - %1").arg(
    ProfileSettings::get()->value("displayName", ProfileSingleApp::instance()->profileId()).toString()
  );

  if (!pageTitle.isEmpty()) {
    title = pageTitle + " - " + title;
  }

  setWindowTitle(title);
}

void MainWindow::initUI() {
  auto *tossaway = new QVBoxLayout(this);
  tossaway->setContentsMargins(0, 0, 0, 0);

  m_rootWidget = new QWidget(this);
  m_rootWidget->setContentsMargins(0, 0, 0, MainWindow::BOTTOM_BAR_HEIGHT); // Bottom padding for the bar
  tossaway->addWidget(m_rootWidget);
  auto *mainLayout = new QHBoxLayout(m_rootWidget);
  mainLayout->setAlignment(Qt::AlignmentFlag::AlignLeft);
  mainLayout->setContentsMargins(0, 0, 0, 0); // Spacing inside the layout itself
  mainLayout->setSpacing(0);

  // sidebar
  m_sidebar = new Sidebar(m_rootWidget);
  mainLayout->addWidget(m_sidebar);

  m_mainContent = new CoverImageWidget(m_rootWidget);
  auto *contentLayout = new QVBoxLayout(m_mainContent);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  m_stackedWidget = new QStackedWidget(m_mainContent);
  contentLayout->addWidget(m_stackedWidget);

  static_cast<CoverImageWidget*>(m_mainContent)->setImagePath(BackgroundLoader::getImage());
  // make m_mainContent expands to the remaining portion of the layout
  m_mainContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  mainLayout->addWidget(m_mainContent);

  addNavigationItem(LucideIcons::house, "Home", SidebarPosition::Top, new HomeScreen())->setSelected(true);
  addNavigationItem(LucideIcons::library, "Mods", SidebarPosition::Top, new ModsScreen());

  addNavigationItem(LucideIcons::settings, "Settings", SidebarPosition::Bottom, new SettingsScreen());
  addActionItem(LucideIcons::users, "Switch user", SidebarPosition::Bottom, showSwitchUserDialog);

  connect(m_sidebar, &Sidebar::selectedItemChanged, this, &MainWindow::onSelectedItemChanged);

  m_rootWidget->show();
}

void MainWindow::onSelectedItemChanged(SidebarItem* oldItem, SidebarItem* newItem) {
  int oidx = oldItem ? m_sidebar->itemPositionOf(oldItem) : -1;
  int nidx = m_sidebar->itemPositionOf(newItem);

  bool ttbDirection = oidx > nidx; // top-to-bottom if true (oldItem pos > newItem pos), else bottom-to-top

  auto thisWidget = m_navigationMap[newItem];
  if (!thisWidget) return;
  m_stackedWidget->setCurrentWidget(thisWidget);

  anime::slideFade(thisWidget, ttbDirection ? anime::SlideDirection::Down : anime::SlideDirection::Up); 

  setPageTitleBar(thisWidget->windowTitle());
}

SidebarItem* MainWindow::addNavigationItem(QIcon icon, std::string name, SidebarPosition position, QWidget* widget) {
  auto item = m_sidebar->addSidebarItem(icon, name, position);

  m_navigationMap[item] = widget;
  m_stackedWidget->addWidget(widget);
  connect(widget, &QWidget::windowTitleChanged, this, [this, widget](){
    if (m_stackedWidget->currentWidget() == widget) {
      setPageTitleBar(widget->windowTitle());
    }
  });
  widget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

  return item;
};

SidebarItem* MainWindow::addActionItem(QIcon icon, std::string name, SidebarPosition position, std::function<void()> onClicked) {
  auto item = m_sidebar->addSidebarItem(icon, name, position, false, false);

  m_actionMap.push_back(item);
  connect(item, &SidebarItem::clicked, this, onClicked);

  return item;
};

void MainWindow::paintEvent(QPaintEvent *event) {
  // Pass 'this' so the QPainter targets this specific QWidget window
  QPainter painter(this);

  QLinearGradient bottomGradient(0, 0, width(), 0);
  bottomGradient.setColorAt(0, QColor("#f28bc1"));
  bottomGradient.setColorAt(1, QColor("#f38ac3"));
  // Set brush color to pink (RGB or hex) with no border line
  painter.setBrush(bottomGradient); // Hot Pink
  painter.setPen(Qt::NoPen);

  // Calculate dynamic dimensions for the rectangle at the bottom
  int rectHeight = MainWindow::BOTTOM_BAR_HEIGHT;
  int rectY = height() - rectHeight; // Position at the bottom edge

  // Draw rectangle: x, y, width, height
  // width() automatically reflects the current window width
  painter.drawRect(0, rectY, width(), rectHeight);
};

bool MainWindow::confirmQuit() {
  // writing this made me wonder if the vfs solution, while do save disk spaces, was actually a good idea..
  // -henrysck
  return QApplication::quitOnLastWindowClosed() || Dialog::showActionDialog(
      this, 
      "Are you sure you want to exit?", 
      "There are mods running, and the launcher has to be kept in background for it to work.", 
      "Closing the launcher will terminate these games. Save the progress if you wish to continue.",
      Dialog::DialogType::YesNo,
      true
  );
}
void MainWindow::setupTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(QIcon(":/app-icon.png"), this);
    auto *trayMenu = new QMenu(this);

    // Option to bring window back to focus
    QAction *showAction = trayMenu->addAction("Show Window");
    QObject::connect(showAction, &QAction::triggered, this, [this]() {
        this->show();
        this->activateWindow();
    });

    // Option to truly exit the application
    QAction *quitAction = trayMenu->addAction("Quit");
    QObject::connect(quitAction, &QAction::triggered, qApp, [this](){
      if (confirmQuit()) QCoreApplication::quit();
    });

    m_trayIcon->setContextMenu(trayMenu);
    m_trayIcon->show();
}
void MainWindow::closeEvent(QCloseEvent *event) {
    // Check if the user is attempting to close via the window manager (or custom state)
    if (isVisible() && !QApplication::quitOnLastWindowClosed()) {
      event->ignore(); // Cancel the close request
      this->hide();    // Send window to background
    } else {
      m_trayIcon->hide();
      event->accept(); // Allow real shutdown if requested elsewhere
    }
}




MainWindow *getMainWindow() {
  for (QWidget *widget : QApplication::topLevelWidgets()) {
    if (auto *mainWin = qobject_cast<MainWindow *>(widget)) {
      return mainWin;
    }
  }
  return nullptr;
}
