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

#include "MainWindow.hpp"
#include "consts.hpp"
#include "screens/Home.hpp"
#include "screens/ModInfo.hpp"
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
#include "utils/SessionManager.hpp"
#include "utils/anime.hpp"
#include "utils/i18n.hpp"


MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  setPageTitleBar("");
  setMinimumSize(QSize(600,400));
  resize(1020, 600); // Set default starting size (Width, Height)

  //m_currentBackgroundImage = getCurrentImage(m_settings.currentBackground);
  initUI();
  setupTrayIcon();
}

void MainWindow::setPageTitleBar(const QString& pageTitle) {
  auto title = QString("VirtualClub Ren'Py Mod Manager - %1").arg(
    ProfileSettings::get()->value(STK_DISPLAYNAME, ProfileSingleApp::instance()->profileId()).toString()
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

  m_router = new Router();
  contentLayout->addWidget(m_router);

  static_cast<CoverImageWidget*>(m_mainContent)->setImagePath(BackgroundLoader::getImage());
  // make m_mainContent expands to the remaining portion of the layout
  m_mainContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  mainLayout->addWidget(m_mainContent);

  addNavigationItem(LucideIcons::house, translateNoop("Sidebar", "Home"), SidebarPosition::Top, "/", new HomeScreen())->setSelected(true);
  addNavigationItem(LucideIcons::library, "Mods", SidebarPosition::Top, "/mods", new ModsScreen(m_router));
  m_router->addRoute("/mods/:id", [this](const RouteParams& params) {
    auto a = new ModInfoScreen(m_router);
    a->setDisplayingMod(ModsIndex::get()->getModByID(
      params["id"].toStdString()
    ));
    return a;
  }, std::bind(&MainWindow::routeTransitionHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

  addNavigationItem(LucideIcons::settings, translateNoop("Sidebar", "Settings"), SidebarPosition::Bottom, "/settings", new SettingsScreen());
  addActionItem(LucideIcons::users, translateNoop("Sidebar", "Switch profile"), SidebarPosition::Bottom, showSwitchUserDialog);

  connect(m_sidebar, &Sidebar::selectedItemChanged, this, &MainWindow::onSelectedItemChanged);

  m_rootWidget->show();
}

void MainWindow::routeTransitionHandler(QWidget* outgoing, QWidget* incoming, std::function<void()> onFinished) {
  anime::SlideDirection direction;

  if (qobject_cast<ModInfoScreen*>(outgoing)) {
    direction = anime::SlideDirection::Right;
  } else if (qobject_cast<ModInfoScreen*>(incoming)) {
    direction = anime::SlideDirection::Left;
  } else {
    SidebarItem* oldItem = outgoing ? outgoing->property("__sidebarItem").value<SidebarItem*>() : nullptr;
    SidebarItem* newItem = incoming ? incoming->property("__sidebarItem").value<SidebarItem*>() : nullptr;
   
    int oidx = oldItem ? m_sidebar->itemPositionOf(oldItem) : -1;
    int nidx = m_sidebar->itemPositionOf(newItem);

    bool ttbDirection = oidx > nidx; // top-to-bottom if true (oldItem pos > newItem pos), else bottom-to-top
    
    direction = ttbDirection ? anime::SlideDirection::Down : anime::SlideDirection::Up;
  }

  auto anim = anime::slideFade(incoming, direction); 

  if (anim) {
    connect(anim, &QAbstractAnimation::finished, this, [this, incoming, onFinished](){
      if (onFinished) onFinished();
    });
  }

  setPageTitleBar(incoming->windowTitle());
}

void addSidebarItemToRetranslate(SidebarItem* item) {
  item->setText(qApp->translate("Sidebar", item->text().toStdString().c_str()));
  item->setProperty("mvcTLText", item->text());
  item->setProperty("mvcTLContext", "Sidebar");
  addWidgetToRetranslateList(item);
}

SidebarItem* MainWindow::addNavigationItem(QIcon icon, QString name, SidebarPosition position, QString route, QWidget* widget) {
  auto item = m_sidebar->addSidebarItem(icon, name, position);
  addSidebarItemToRetranslate(item);

  m_navigationMap[item] = route;
  widget->setProperty("__sidebarItem", QVariant::fromValue<void*>(item));
  m_router->addFixedRoute(route, widget, std::bind(&MainWindow::routeTransitionHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  connect(widget, &QWidget::windowTitleChanged, this, [this, widget](){
    if (widget->isVisible())
    //if (m_stackedWidget->currentWidget() == widget) {
      setPageTitleBar(widget->windowTitle());
    //}
  });
  widget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

  return item;
};

SidebarItem* MainWindow::addActionItem(QIcon icon, QString name, SidebarPosition position, std::function<void()> onClicked) {
  auto item = m_sidebar->addSidebarItem(icon, name, position, false, false);
  addSidebarItemToRetranslate(item);

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
#define trConfirmQuit(t) qApp->translate("ConfirmQuitDialog", t)
  return SessionManager::hasMountedMod() && Dialog::showActionDialog(
      this, 
      trConfirmQuit("Are you sure want to exit?"),
      SessionManager::hasPlayingMod() 
      ? trConfirmQuit("There are mods running, and the launcher has to be kept in background for it to work.")
      : trConfirmQuit("There are mounted directories which might be in use, and the launcher has to be kept in background for it to exists."),
      SessionManager::hasPlayingMod()
      ? trConfirmQuit("Closing the launcher will terminate these games. Save the progress if you wish to continue.")
      : trConfirmQuit("Closing the launcher will remove these directories. Save the progress if you wish to continue."),
      Dialog::DialogType::YesNo,
      true
  );
#undef trConfirmQuit
}

void MainWindow::updateTrayTooltip() {
  if (!m_trayIcon) return;

  QString sessionStatus;

  if (SessionManager::hasMountedMod()) {
    sessionStatus = QString("%1 mounted").arg(SessionManager::countMountedMods());

    if (SessionManager::hasPlayingMod()) {
      sessionStatus += QString(", %1 running").arg(SessionManager::countPlayingMods());
    }
  }

  m_trayIcon->setToolTip(QString("VirtualClub - %1%2").arg(
    ProfileSingleApp::instance()->profileName(),
    sessionStatus
  ));
}

void MainWindow::setupTrayIcon() {
  m_trayIcon = new QSystemTrayIcon(QIcon(":/app-icon.png"), this);
  updateTrayTooltip();
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

  QObject::connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) { // Left click
      this->show();
      this->activateWindow();
    }
  });

  m_trayIcon->setContextMenu(trayMenu);
  m_trayIcon->show();
}
void MainWindow::closeEvent(QCloseEvent *event) {
    // Check if the user is attempting to close via the window manager (or custom state)
    if (isVisible() && SessionManager::hasMountedMod()) {
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
