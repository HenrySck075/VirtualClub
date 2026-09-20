#include "ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include <QDirIterator>
#include <QDir>
#include <QSettings>
#include <mutex>

std::shared_ptr<YamlSettings> ProfileSettings::get() {
  return getOf(ProfileSingleApp::instance()->profileId());
}


std::shared_ptr<YamlSettings> ProfileSettings::getOf(const QString &name) {
  static std::unordered_map<std::string, std::weak_ptr<YamlSettings>> registry;
  static std::mutex mutex;

  std::lock_guard<std::mutex> lock(mutex);
  std::string key = name.toStdString();

  // Re-use existing instance if active
  if (auto instance = registry[key].lock()) {
    return instance;
  }

  // Create new shared instance if non-existent or expired
  auto newInstance =
      std::make_shared<YamlSettings>(QString("profiles/%1.yaml").arg(key));
  registry[key] = newInstance;
  return newInstance;
}


QList<QString> ProfileSettings::list() {
  QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

  auto profilesPath = QDir(configDir).filePath("profiles/");
  
  QDirIterator it(profilesPath, QStringList() << "*.yaml", QDir::Files);

  QList<QString> profileNames;
  while (it.hasNext()) {
    QString filePath = it.next();
    QString fileName = QFileInfo(filePath).baseName(); // Get the base name without extension
    profileNames.append(fileName);
  }

  return profileNames;
}
