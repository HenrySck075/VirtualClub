#pragma once

#include <functional>

class QWidget;
class QSystemTrayIcon;

namespace TrayIcon {
  void setup(QWidget* mainWindow, std::function<bool()> confirmQuit);
  void updateTooltip();
  QSystemTrayIcon* get();
}
