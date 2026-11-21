#ifndef SCREENS_Home_H
#define SCREENS_Home_H

#include <QWidget>
#include <nlohmann/json.hpp>

class Router;
class FlowLayout;
class AutoUpdater;

class HomeScreen : public QWidget {
  Q_OBJECT
public:
  explicit HomeScreen(Router* router, QWidget *parent = nullptr);
  void showUpdateAvailable(const nlohmann::json& manifest);

private:
  void updateRecentlyPlayed();

  Router* m_router;
  QWidget* m_cards;
  FlowLayout* m_cardsLayout;
  class QFrame* m_updateRow = nullptr;
  class QLabel* m_updateLabel = nullptr;
  AutoUpdater* m_updater = nullptr;
  nlohmann::json m_updateManifest;
};

#endif
