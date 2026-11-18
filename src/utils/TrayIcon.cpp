#include "TrayIcon.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWidget>

#include "ProfileSingleApp.hpp"
#include "SessionManager.hpp"
#include <QDebug>

namespace {
QSystemTrayIcon* trayIcon = nullptr;
}

void TrayIcon::updateTooltip() {
  if (!trayIcon) {
    qDebug() << "TrayIcon: tooltip update ignored; tray not initialized";
    return;
  }

  QString sessionStatus;

  if (SessionManager::hasMountedMod()) {
    sessionStatus = QString("%1 mounted").arg(SessionManager::countMountedMods());

    if (SessionManager::hasPlayingMod()) {
      sessionStatus += QString(", %1 running").arg(SessionManager::countPlayingMods());
    }
  }

  if (!sessionStatus.isEmpty()) sessionStatus = "(" + sessionStatus + ")";

  trayIcon->setToolTip(QString("VirtualClub - %1%2").arg(
    ProfileSingleApp::instance()->profileName(),
    sessionStatus
  ));
  qDebug() << "TrayIcon: tooltip updated:" << trayIcon->toolTip();
}

void TrayIcon::setup(QWidget* mainWindow, std::function<bool()> confirmQuit) {
  trayIcon = new QSystemTrayIcon(QIcon(":/app-icon.png"), mainWindow);
  updateTooltip();
  auto* trayMenu = new QMenu(mainWindow);

  QAction* showAction = trayMenu->addAction("Show Window");
  QObject::connect(showAction, &QAction::triggered, mainWindow, [mainWindow]() {
    mainWindow->show();
    mainWindow->activateWindow();
  });

  QAction* quitAction = trayMenu->addAction("Quit");
  QObject::connect(quitAction, &QAction::triggered, qApp, [confirmQuit]() {
    if (confirmQuit()) QCoreApplication::quit();
  });

  QObject::connect(trayIcon, &QSystemTrayIcon::activated, mainWindow,
    [mainWindow](QSystemTrayIcon::ActivationReason reason) {
      if (reason == QSystemTrayIcon::Trigger) {
        mainWindow->show();
        mainWindow->activateWindow();
      }
    });

  trayIcon->setContextMenu(trayMenu);
  trayIcon->show();
}

QSystemTrayIcon* TrayIcon::get() {
  return trayIcon;
}
