#include "AutoUpdater.hpp"

#include "consts.hpp"
#include "ui/MESWidgets.hpp"
#include "MainWindow.hpp"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSettings>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <filesystem>

namespace {
QString platformName() {
#ifdef Q_OS_WIN
  return "windows";
#else
  return "linux";
#endif
}

std::array<int, 3> parseStableVersion(QString version) {
  version.remove(QRegularExpression("^v"));
  std::array<int, 3> result{0, 0, 0};
  const auto parts = version.split('.');
  for (int i = 0; i < 3 && i < parts.size(); ++i) {
    bool ok = false;
    result[i] = parts[i].toInt(&ok);
    if (!ok) result[i] = 0;
  }
  return result;
}
}

AutoUpdater::AutoUpdater(QObject* parent)
  : QObject(parent), m_manager(new QNetworkAccessManager(this)) {}

bool AutoUpdater::isNewerThanCurrent(const nlohmann::json& manifest) {
  const QString channel = QString::fromStdString(manifest.value("channel", ""));
  const QString currentChannel = QStringLiteral(MVC_CHANNEL);
  if (channel == "local" || currentChannel == "local") {
    return false;
  }
  const QString version = QString::fromStdString(manifest.value("version", ""));
  const QString minimumVersion =
      QString::fromStdString(manifest.value("minimum_launcher_version", "0.0.0"));
  if (parseStableVersion(QStringLiteral(MVC_LAUNCHER_VERSION)) <
      parseStableVersion(minimumVersion)) {
    return false;
  }

  if (channel == "stable" && currentChannel == "stable") {
    const auto current = parseStableVersion(QStringLiteral(MVC_LAUNCHER_VERSION));
    const auto available = parseStableVersion(version);
    return available > current;
  }

  if (channel == "nightly") {
    const QString buildId = QString::fromStdString(manifest.value("build_id", ""));
    return !buildId.isEmpty() && buildId != QStringLiteral(MVC_BUILD_ID);
  }

  return channel == "stable" && currentChannel == "nightly";
}

void AutoUpdater::checkForUpdate() {
  fetchManifest();
}

void AutoUpdater::fetchManifest() {
  QSettings settings;
  const QString channel =
      settings.value(GSTK_UPDATE_CHANNEL, QStringLiteral(MVC_CHANNEL)).toString();
  if (channel == "local") {
    emit noUpdate();
    return;
  }
  QUrl url;
  if (channel == "nightly") {
    url = QUrl("https://github.com/HenrySck075/VirtualClub/releases/download/nightly/update-manifest-" + platformName() + ".json");
  } else {
    url = QUrl("https://github.com/HenrySck075/VirtualClub/releases/latest/download/update-manifest-" + platformName() + ".json");
  }

  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader, "VirtualClub-Updater");
  auto* reply = m_manager->get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit checkFailed(reply->errorString());
      return;
    }
    try {
      auto manifest = nlohmann::json::parse(reply->readAll().toStdString());
      const QString manifestPlatform =
          QString::fromStdString(manifest.value("platform", ""));
      if (manifestPlatform != platformName()) {
        emit checkFailed(tr("The update manifest does not match this platform."));
        return;
      }
      if (!isNewerThanCurrent(manifest)) {
        emit noUpdate();
        return;
      }
      emit updateAvailable(manifest);
    } catch (const std::exception& error) {
      emit checkFailed(QString::fromLatin1(error.what()));
    }
  });
}

void AutoUpdater::downloadUpdate(const nlohmann::json& manifest) {
  const QString url = QString::fromStdString(manifest.value("archive_url", ""));
  const QString expectedHash = QString::fromStdString(manifest.value("sha256", "")).toLower();
  if (url.isEmpty() || expectedHash.size() != 64) {
    emit checkFailed(tr("The update manifest is incomplete."));
    return;
  }

  QTemporaryDir tempDir;
  tempDir.setAutoRemove(false);
  if (!tempDir.isValid()) {
    emit checkFailed(tr("Could not create an update staging directory."));
    return;
  }

  const QString archiveName = QString::fromStdString(manifest.value("archive", "VirtualClub-update"));
  const QString archivePath = tempDir.filePath(archiveName);
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader, "VirtualClub-Updater");
  auto* reply = m_manager->get(request);

  auto* progress = new QProgressDialog(tr("Downloading update..."), tr("Cancel"), 0, 1);
  progress->setWindowModality(Qt::WindowModal);
  progress->setAutoClose(false);
  progress->setAutoReset(false);
  progress->show();

  connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
  connect(reply, &QNetworkReply::downloadProgress, progress,
          [progress](qint64 received, qint64 total) {
            progress->setMaximum(total > 0 ? static_cast<int>(total) : 0);
            progress->setValue(static_cast<int>(received));
          });

  connect(reply, &QNetworkReply::finished, this, [this, reply, progress, archivePath, expectedHash, manifest]() mutable {
    progress->deleteLater();
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      emit checkFailed(reply->errorString());
      return;
    }

    QSaveFile file(archivePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(reply->readAll()) == -1 || !file.commit()) {
      emit checkFailed(tr("Could not save the downloaded update."));
      return;
    }

    QFile digestFile(archivePath);
    if (!digestFile.open(QIODevice::ReadOnly)) {
      emit checkFailed(tr("Could not verify the downloaded update."));
      return;
    }
    if (manifest.contains("archive_size") &&
        digestFile.size() != manifest["archive_size"].get<qint64>()) {
      emit checkFailed(tr("Update verification failed: archive size mismatch."));
      return;
    }
    QCryptographicHash digest(QCryptographicHash::Sha256);
    while (!digestFile.atEnd()) {
      digest.addData(digestFile.read(1024 * 1024));
    }
    const QString actualHash = digest.result().toHex();
    if (actualHash != expectedHash) {
      emit checkFailed(tr("Update verification failed: SHA-256 mismatch."));
      return;
    }
    startUpdate(manifest, archivePath);
  });
}

void AutoUpdater::startUpdate(const nlohmann::json& manifest, const QString& archivePath) {
  const QString installDir = QCoreApplication::applicationDirPath();
  const QString appExe = QCoreApplication::applicationFilePath();
  const QString pid = QString::number(QCoreApplication::applicationPid());

  QString helper = installDir + "/VirtualClub-Updater";
#ifdef Q_OS_WIN
  helper += ".exe";
#endif
  if (!QFileInfo::exists(helper)) {
    emit checkFailed(tr("The update helper is missing."));
    return;
  }

  QString launchHelper = helper;
#ifdef Q_OS_WIN
  const QString tempHelperDir = QDir::temp().filePath(
      "VirtualClub-Updater-" + QString::number(QRandomGenerator::global()->generate()));
  QDir().mkpath(tempHelperDir);
  launchHelper = tempHelperDir + "/VirtualClub-Updater.exe";
  if (!QFile::copy(helper, launchHelper)) {
    emit checkFailed(tr("Could not prepare the update helper."));
    return;
  }
#endif

  QStringList args{
      "--archive", archivePath,
      "--install-dir", installDir,
      "--app", appExe,
      "--wait-pid", pid,
      "--preserve-assets"
  };

  if (!QProcess::startDetached(launchHelper, args)) {
    emit checkFailed(tr("Could not launch the update helper."));
  }
}
