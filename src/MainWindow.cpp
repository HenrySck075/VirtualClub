#include <QPainter>
#include <QLayout>
#include <QScrollArea>
#include <QEvent>
#include <QCoreApplication>
#include <QApplication>
#include <QTime>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QSystemTrayIcon>

#include "MainWindow.hpp"
#include "consts.hpp"
#include "screens/Home.hpp"
#include "screens/GlobalSettings.hpp"
#include "screens/ModInfo.hpp"
#include "screens/Mods.hpp"
#include "screens/OnlineModsIndex.hpp"
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
#include "utils/TrayIcon.hpp"
#include "utils/anime.hpp"
#include "utils/i18n.hpp"


MainWindow::MainWindow(QWidget *parent) : QWidget(parent), m_topLevelNavSfx(this), m_subLevelNavSfx(this) {
  setPageTitleBar("");
  setMinimumSize(QSize(600,400));
  resize(1140, 600); // Set default starting size (Width, Height)

  //m_currentBackgroundImage = getCurrentImage(m_settings.currentBackground);
  initUI();
  TrayIcon::setup(this, [this]() {
    return confirmQuit();
  });
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

void MainWindow::setBackgroundPack(const QString& pack) {
  if (auto* background = qobject_cast<CoverImageWidget*>(m_mainContent)) {
    background->setImagePath(BackgroundLoader::getImage(pack));
  }
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


  m_topLevelNavSfx.setSource(QUrl("qrc:/audio/sidebar_click.wav"));
  m_topLevelNavSfx.setMuted(false);
  m_subLevelNavSfx.setSource(QUrl("qrc:/audio/navigation.wav"));
  m_subLevelNavSfx.setMuted(false);


  m_homeScreen = new HomeScreen(m_router);
  addNavigationItem(LucideIcons::house, QT_TRANSLATE_NOOP("Sidebar", "Home"), SidebarPosition::Top, "/home", m_homeScreen)->setSelected(true);
  addNavigationItem(LucideIcons::library, "Mods", SidebarPosition::Top, "/mods", new ModsScreen(m_router));
  m_router->addFixedRoute("/mods/online", new OnlineModsIndexScreen(m_router), std::bind(&MainWindow::routeTransitionHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  m_router->addRoute("/mods/:id", [this](const RouteParams& params) {
    auto a = new ModInfoScreen(m_router);
    a->setDisplayingMod(ModsIndex::get()->getModByID(
      params["id"].toStdString()
    ));
    return a;
  }, std::bind(&MainWindow::routeTransitionHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  addNavigationItem(LucideIcons::settings, QT_TRANSLATE_NOOP("Sidebar", "Settings"), SidebarPosition::Bottom, "/settings", new SettingsScreen(m_router));
  m_router->addRoute("/settings/global", [this](const RouteParams&) {
    return new GlobalSettingsScreen(m_router);
  }, std::bind(&MainWindow::routeTransitionHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  addActionItem(LucideIcons::users, QT_TRANSLATE_NOOP("Sidebar", "Switch profile"), SidebarPosition::Bottom, showSwitchUserDialog);
  m_router->navigate("/home");

  //connect(m_sidebar, &Sidebar::selectedItemChanged, this, &MainWindow::onSelectedItemChanged);

  m_rootWidget->show();
}

QString removeSuffix(const QString& str, const QString& suffix) {
  if (str.endsWith(suffix)) {
    return str.left(str.length() - suffix.length());
  }
  return str;
}

SidebarItem* MainWindow::sidebarItemForPath(QString& path) {
  // check against only the top-level path segment
  QString topLevelPath = "/" + path.split("/").value(1, "");

  for (auto& [item, route] : m_navigationMap) {
    if (route == topLevelPath) {
      return item;
    }
  }
  return nullptr;
}

void MainWindow::showUpdateAvailable(const nlohmann::json& manifest) {
  if (m_homeScreen) {
    m_homeScreen->showUpdateAvailable(manifest);
  }
  if (m_router) {
    m_router->navigate("/home");
  }
}

std::unordered_map<QWidget*, QMetaObject::Connection> g_titleBarUpdaterConnections;
void MainWindow::routeTransitionHandler(QString path, QWidget* outgoing, QWidget* incoming, std::function<void()> onFinished) {
  anime::SlideDirection direction;

  QString lastPath = removeSuffix(m_currentPath, "/");
  m_currentPath = removeSuffix(path, "/");

  if (m_currentPath == lastPath) return; 

  auto* sfx = &m_topLevelNavSfx;

  // since lastPath and path are guaranteed not to be equal here, 
  // these checks can be used safely without implementing more guards
  if (lastPath.startsWith(path)) {
    direction = anime::SlideDirection::Right;
    sfx = &m_subLevelNavSfx;
  } else if (path.startsWith(lastPath)) {
    direction = anime::SlideDirection::Left;
    sfx = &m_subLevelNavSfx;
  } else {
    SidebarItem* oldItem = sidebarItemForPath(lastPath);
    SidebarItem* newItem = sidebarItemForPath(path);
   
    int oidx = oldItem ? m_sidebar->itemPositionOf(oldItem) : -1;
    int nidx = m_sidebar->itemPositionOf(newItem);
    if (oidx == nidx) return;

    bool ttbDirection = oidx > nidx; // top-to-bottom if true (oldItem pos > newItem pos), else bottom-to-top
    
    direction = ttbDirection ? anime::SlideDirection::Down : anime::SlideDirection::Up;

    if (newItem) {
      oldItem->setSelected(false);
      newItem->setProperty("__mvcSkipNavigate", true);
      newItem->setSelected(true);
      newItem->setProperty("__mvcSkipNavigate", false);
    }
  }

  auto anim = anime::slideFade(incoming, direction); 

  if (anim) {
    connect(anim, &QAbstractAnimation::finished, this, [this, incoming, onFinished](){
      if (onFinished) onFinished();
    });
  }

  setPageTitleBar(incoming->windowTitle());
  sfx->play();

  // disconnect old one if exists
  // meaning g_titleBarUpdaterConnections in practice should have the size of however many MainWindow are there per process (1)
  if (g_titleBarUpdaterConnections.find(outgoing) != g_titleBarUpdaterConnections.end()) {
    disconnect(g_titleBarUpdaterConnections[outgoing]);
    g_titleBarUpdaterConnections.erase(outgoing);
  }
  g_titleBarUpdaterConnections[incoming] = connect(incoming, &QWidget::windowTitleChanged, this, [this, incoming](){
    if (m_router == incoming) {
      setPageTitleBar(incoming->windowTitle());
    }
  });
}

void addSidebarItemToRetranslate(SidebarItem* item, const QString& name) {
  addRetranslateCallback(item, [item, name] {
    item->setText(qApp->translate("Sidebar", name.toStdString().c_str()));
  });
}

SidebarItem* MainWindow::addNavigationItem(LucideIcon icon, QString name, SidebarPosition position, QString route, QWidget* widget) {
  auto item = m_sidebar->addSidebarItem(icon, name, position);
  addSidebarItemToRetranslate(item, name);

  m_navigationMap[item] = route;
  widget->setProperty("__sidebarItem", QVariant::fromValue<SidebarItem*>(item));
  m_router->addFixedRoute(route, widget, std::bind(&MainWindow::routeTransitionHandler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  connect(item, &SidebarItem::clicked, this, [this, route, item](){
    if (item->property("__mvcSkipNavigate").toBool()) return;
    m_router->navigate(route);
  });
  connect(widget, &QWidget::windowTitleChanged, this, [this, widget](){
    if (widget->isVisible())
    //if (m_stackedWidget->currentWidget() == widget) {
      setPageTitleBar(widget->windowTitle());
    //}
  });
  widget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

  return item;
};

SidebarItem* MainWindow::addActionItem(LucideIcon icon, QString name, SidebarPosition position, std::function<void()> onClicked) {
  auto item = m_sidebar->addSidebarItem(icon, name, position, false, false);
  addSidebarItemToRetranslate(item, name);

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

void MainWindow::closeEvent(QCloseEvent *event) {
    // Check if the user is attempting to close via the window manager (or custom state)
    if (isVisible() && SessionManager::hasMountedMod()) {
      event->ignore(); // Cancel the close request
      this->hide();    // Send window to background
    } else {
      if (auto* trayIcon = TrayIcon::get()) trayIcon->hide();
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
