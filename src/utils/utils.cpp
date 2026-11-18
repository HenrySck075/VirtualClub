#include "utils.hpp"
#include "MainWindow.hpp"
#include "consts.hpp"
#include "utils/ProfileSettings.hpp"
#include <QFileDialog>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkDiskCache>
#include <QStandardPaths>

#include <qmetaobject.h>
#include <random>
#include <sstream>
#include <QDebug>

#include "macros.h"
#ifdef MVC_DEBUG
#include <cpptrace/from_current_macros.hpp>
#include <cpptrace/from_current.hpp>
#endif

int findChildWidgetIndex(QLayout* layout, QWidget* child) {
    if (!layout) return -1;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        // Check if the item is a QWidget
        if (QWidget* widget = item->widget()) {
            if (widget == child) {
                return i;
            }
        }
        
        /*
        // Optional: Recurse into sub-layouts
        if (QLayout* childLayout = item->layout()) {
            if (QWidget* found = findChildWidgetBy(childLayout, predicate)) {
                return found;
            }
        }
        */
    }

    return -1;
}
QWidget* findChildWidgetBy(QLayout* layout, std::function<bool(QWidget*)> predicate) {
    if (!layout) return nullptr;

    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (!item) continue;

        // Check if the item is a QWidget
        if (QWidget* widget = item->widget()) {
            if (predicate(widget)) {
                return widget;
            }
        }
        
        /*
        // Optional: Recurse into sub-layouts
        if (QLayout* childLayout = item->layout()) {
            if (QWidget* found = findChildWidgetBy(childLayout, predicate)) {
                return found;
            }
        }
        */
    }

    return nullptr;
}

bool askForBasePathChange() {
  qDebug() << "Utils: prompting for base game path";
  auto settings = ProfileSettings::get();
  auto directory = QFileDialog::getExistingDirectory(
      nullptr, "Select a base game directory.");
  if (directory != "") {
    qDebug() << "Utils: base game path selected" << directory;
    settings->setValue(STK_BASEPATH, directory);
    return true;
  }
  qDebug() << "Utils: base game path selection cancelled";
  return false;
}

bool askForProfilePictureChange() {
  qDebug() << "Utils: prompting for profile picture";
  auto pfpImagePath = QFileDialog::getOpenFileName(getMainWindow(), "Select a profile picture", "", "Images (*.png *.jpg *.jpeg *.bmp)");
  if (pfpImagePath.isEmpty()) {
    qDebug() << "Utils: profile picture selection cancelled";
    return false;
  }
  auto pfpStore = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()) / "profile_pictures";
  std::filesystem::create_directories(pfpStore);
  auto pfpFileName = std::filesystem::path(pfpImagePath.toStdString()).filename();
  auto pfpDestPath = pfpStore / pfpFileName;
  std::filesystem::copy_file(pfpImagePath.toStdString(), pfpDestPath, std::filesystem::copy_options::overwrite_existing);

  ProfileSettings::get()->setValue(STK_PFP, QString::fromStdString(pfpDestPath.string()));
  qDebug() << "Utils: profile picture set to" << QString::fromStdString(pfpDestPath.string());
  return true;
}


std::string generate_uuid_v4() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dis;

    uint32_t data[4] = { dis(gen), dis(gen), dis(gen), dis(gen) };

    // Set UUID version 4 (bits 12-15 of time_hi_and_version to 0100)
    data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000;
    // Set UUID variant (bits 6-7 of clock_seq_hi_and_reserved to 10)
    data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;

    std::ostringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << data[0] << "-"
       << std::setw(4) << (data[1] >> 16) << "-"
       << std::setw(4) << (data[1] & 0xFFFF) << "-"
       << std::setw(4) << (data[2] >> 16) << "-"
       << std::setw(4) << (data[2] & 0xFFFF)
       << std::setw(8) << data[3];
    std::string result = ss.str();
    qDebug() << "Utils: generated UUID" << QString::fromStdString(result);
    return result;
}

bool createDesktopShortcut(const QString &shortcutName,
                           const QString &launchArgument,
                           const QString &iconPath) {
  qDebug() << "Utils: creating desktop shortcut" << shortcutName
           << "argument" << launchArgument << "icon" << iconPath;
  QString executablePath = QCoreApplication::applicationFilePath();

#if defined(Q_OS_WIN)
  QString desktopPath =
      QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  QString linkPath = QDir(desktopPath).filePath(shortcutName + ".lnk");

  // Construct PowerShell command to create a Windows shortcut (.lnk) via
  // WScript.Shell
  QString psCommand =
      QString(
          "$s = (New-Object -ComObject WScript.Shell).CreateShortcut('%1'); "
          "$s.TargetPath = '%2'; "
          "$s.Arguments = '%3'; "
          "$s.IconLocation = '%4'; "
          "$s.Save();")
          .arg(linkPath, executablePath, launchArgument,
               iconPath.isEmpty() ? executablePath : iconPath);

  const int exitCode = QProcess::execute("powershell", {"-NoProfile", "-NonInteractive",
                                          "-Command", psCommand});
  qDebug() << "Utils: PowerShell shortcut creation result" << exitCode;
  return exitCode == 0;

#elif defined(Q_OS_LINUX)
  QString desktopPath =
      QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  QString desktopFilePath =
      QDir(desktopPath).filePath(shortcutName + ".desktop");

  // Construct standard XDG desktop entry content
  QString content =
      QString("[Desktop Entry]\n"
              "Type=Application\n"
              "Name=%1\n"
              "Exec=\"%2\" %3\n"
              "Icon=%4\n"
              "Terminal=false\n")
          .arg(shortcutName, executablePath, launchArgument, iconPath);

  QFile file(desktopFilePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Utils: could not create desktop shortcut file" << desktopFilePath;
    return false;
  }

  file.write(content.toUtf8());
  file.close();

  // Mark file as executable
  const bool result = file.setPermissions(QFile::ExeUser | QFile::ReadUser |
                             QFile::WriteUser | QFile::ReadGroup |
                             QFile::ReadOther);
  qDebug() << "Utils: desktop shortcut permissions set" << result;
  return result;

#else
  return false;
#endif
}
QNetworkAccessManager* createNetworkAccessManager(QObject *context) {
  qDebug() << "Utils: creating network access manager";
  auto *manager = new QNetworkAccessManager(context);
  QNetworkDiskCache* diskCache = new QNetworkDiskCache(context);

  QString cacheDir =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      "/network_cache";
  diskCache->setCacheDirectory(cacheDir);
  diskCache->setMaximumCacheSize(50 * 1024 * 1024);
  manager->setCache(diskCache);
  qDebug() << "Utils: network cache directory" << cacheDir;

  return manager;
}



QFuture<QByteArray> fetchUrlAsync(QNetworkAccessManager* manager, const QUrl& url) {
  qDebug() << "Utils: starting async URL fetch" << url;
  auto promise = std::make_shared<QPromise<QByteArray>>();
  QFuture<QByteArray> future = promise->future();

  QNetworkRequest request(url);
  request.setHeader(
    QNetworkRequest::UserAgentHeader, 
    "VirtualClub v" MVC_VERSION " (Qt" QT_VERSION_STR ")"
  );
  QNetworkReply* reply = manager->get(request);

  QObject::connect(reply, &QNetworkReply::finished, [url, reply, promise]() {
    qDebug() << "Utils: async URL fetch finished" << url
             << "status" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
             << "bytes" << reply->bytesAvailable();
    promise->start();
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      qDebug() << "Utils: URL fetch succeeded, bytes" << data.size();
      promise->addResult(data);
    } else {
      qWarning() << "Utils: URL fetch failed" << url << reply->errorString();
      promise->setException(network_error(reply->error(), url.toString()));
    }
    promise->finish();
    
    reply->deleteLater();
  });

  return future;
}

QFuture<nlohmann::json> fetchJsonAsync(QNetworkAccessManager* manager, const QUrl& url) {
  qDebug() << "Utils: starting async JSON fetch" << url;
  auto promise = std::make_shared<QPromise<nlohmann::json>>();
  QFuture<nlohmann::json> future = promise->future();

  QNetworkRequest request(url);
  // fun fact the ai autocompleter sets the user agent
  // to sum shit like "Genshin Studio" idk why
  request.setHeader(
    QNetworkRequest::UserAgentHeader, 
    "VirtualClub v" MVC_VERSION " (Qt" QT_VERSION_STR ")"
  );
  QNetworkReply* reply = manager->get(request);

  QObject::connect(reply, &QNetworkReply::finished, [reply, promise, url]() {
    qDebug() << "Utils: async JSON fetch finished" << url
             << "status" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    promise->start();
    if (reply->error() == QNetworkReply::NoError) {
      CPPTRACE_TRY {
        nlohmann::json jsonData = nlohmann::json::parse(reply->readAll().toStdString());
        qDebug() << "Utils: JSON fetch parsed successfully";
        promise->addResult(jsonData);
      } CPPTRACE_CATCH (...) {
        qWarning() << "Utils: failed to parse JSON response from" << url;
        promise->setException(std::current_exception()); 
        MVC_DEBUG_EVAL(cpptrace::from_current_exception().print());
      }
    } else {
      qWarning() << "Utils: JSON fetch failed" << url << reply->errorString();
      promise->setException(network_error(reply->error(), url.toString()));
    }
    promise->finish();
    
    reply->deleteLater();
  });

  return future;
}

const char* network_error::what() const noexcept {
  return msg.c_str();
}
network_error::network_error(QNetworkReply::NetworkError error, QString url)
    : error_(error), url_(std::move(url)) {
  msg = std::string("Network error ") + QMetaEnum::fromType<QNetworkReply::NetworkError>().valueToKey(error_) +
         " occurred while fetching URL: " + url_.toStdString();
}
