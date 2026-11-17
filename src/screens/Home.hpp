#ifndef SCREENS_Home_H
#define SCREENS_Home_H

#include <QWidget>

class Router;
class FlowLayout;

class HomeScreen : public QWidget {
  Q_OBJECT
public:
  explicit HomeScreen(Router* router, QWidget *parent = nullptr);

private:
  void updateRecentlyPlayed();

  Router* m_router;
  QWidget* m_cards;
  FlowLayout* m_cardsLayout;
};

#endif
