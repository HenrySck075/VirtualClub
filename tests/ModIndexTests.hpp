#pragma once

#include <QObject>

class ModIndexTests : public QObject {
  Q_OBJECT

private slots:
  void deserializesAndPersistsMods();
  void updatesAndRemovesMods();
  void recentlyPlayedIsUniqueAndLimited();
};
