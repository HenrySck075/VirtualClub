#include "ProfileSettings.hpp"
#include <QDirIterator>
#include <QDir>
#include <QSettings>
#include <mutex>

std::shared_ptr<YamlSettings> ProfileSettings::get() {
  QSettings qs;
  return getOf(qs.value("currentProfile", "default").toString());
}

void ProfileSettings::setActiveProfile(const QString& profileId) {
  QSettings qs;
  qs.setValue("currentProfile", profileId);
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
