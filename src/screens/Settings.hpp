#ifndef SCREENS_Settings_H
#define SCREENS_Settings_H

#include <QWidget>

class Router;

class SettingsScreen : public QWidget {
  Q_OBJECT
public:
  explicit SettingsScreen(Router *router = nullptr, QWidget *parent = nullptr);
};

#endif
