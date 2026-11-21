#pragma once

#include <QObject>
#include <QString>

#include <nlohmann/json.hpp>

class QNetworkAccessManager;
class QNetworkReply;

class AutoUpdater : public QObject {
  Q_OBJECT
public:
  explicit AutoUpdater(QObject* parent = nullptr);

  void checkForUpdate();
  void downloadUpdate(const nlohmann::json& manifest);

  static bool isNewerThanCurrent(const nlohmann::json& manifest);

signals:
  void updateAvailable(const nlohmann::json& manifest);
  void noUpdate();
  void checkFailed(const QString& error);

private:
  QNetworkAccessManager* m_manager = nullptr;
  void fetchManifest();
  void startUpdate(const nlohmann::json& manifest, const QString& archivePath);
};
