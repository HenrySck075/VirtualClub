#ifndef SCREENS_GlobalSettings_H
#define SCREENS_GlobalSettings_H

#include <QWidget>

class Router;

class GlobalSettingsScreen : public QWidget {
  Q_OBJECT
public:
  explicit GlobalSettingsScreen(Router *router = nullptr, QWidget *parent = nullptr);
};

#endif
