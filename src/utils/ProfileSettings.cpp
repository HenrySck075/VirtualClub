#include "ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include <QDirIterator>
#include <QDir>
#include <QSettings>
#include <mutex>
#include <QDebug>

std::shared_ptr<YamlSettings> ProfileSettings::get() {
  return getOf(ProfileSingleApp::instance()->profileId());
}


std::shared_ptr<YamlSettings> ProfileSettings::getOf(const QString &name) {
  qDebug() << "ProfileSettings: resolving profile" << name;
  static std::unordered_map<std::string, std::weak_ptr<YamlSettings>> registry;
  static std::mutex mutex;

  std::lock_guard<std::mutex> lock(mutex);
  std::string key = name.toStdString();

  // Re-use existing instance if active
  if (auto instance = registry[key].lock()) {
    qDebug() << "ProfileSettings: reusing cached settings for" << name;
    return instance;
  }

  // Create new shared instance if non-existent or expired
  auto newInstance =
      std::make_shared<YamlSettings>(QString("profiles/%1.yaml").arg(key));
  registry[key] = newInstance;
  qDebug() << "ProfileSettings: created settings for" << name;
  return newInstance;
}


QList<QString> ProfileSettings::list() {
  qDebug() << "ProfileSettings: listing profiles";
  QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

  auto profilesPath = QDir(configDir).filePath("profiles/");
  
  QDirIterator it(profilesPath, QStringList() << "*.yaml", QDir::Files);

  QList<QString> profileNames;
  while (it.hasNext()) {
    QString filePath = it.next();
    QString fileName = QFileInfo(filePath).baseName(); // Get the base name without extension
    profileNames.append(fileName);
  }
  qDebug() << "ProfileSettings: found" << profileNames.size() << "profiles";
  return profileNames;
}

void ProfileSettings::deleteProfile(const QString& name) {
  qDebug() << "ProfileSettings: deleting profile" << name;
  if (name == ProfileSingleApp::instance()->profileId()) {
    qWarning() << "ProfileSettings: refusing to delete active profile" << name;
    return;
  }

  QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QString profileFilePath = QDir(configDir).filePath(QString("profiles/%1.yaml").arg(name));

  if (QFile::exists(profileFilePath)) {
    QFile::remove(profileFilePath);
  }

  QString modsFilePath = QDir(configDir).filePath(QString("profiles/mods/%1.yaml").arg(name));
  if (QFile::exists(modsFilePath)) {
    QFile::remove(modsFilePath);
  }
}
