#ifndef SCREENS_ModInfo_H
#define SCREENS_ModInfo_H

#include <QWidget>
class ModInfoScreen : public QWidget {
  Q_OBJECT
public:
  explicit ModInfoScreen(QWidget *parent = nullptr);
signals:
  void backButtonClicked();
};

#endif
