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
#include <QStandardPaths>

#include <random>
#include <sstream>

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
  auto settings = ProfileSettings::get();
  auto directory = QFileDialog::getExistingDirectory(
      nullptr, "Select a base game directory.");
  if (directory != "") {
    settings->setValue(STK_BASEPATH, directory);
    return true;
  }
  return false;
}

bool askForProfilePictureChange() {
  auto pfpImagePath = QFileDialog::getOpenFileName(getMainWindow(), "Select a profile picture", "", "Images (*.png *.jpg *.jpeg *.bmp)");
  if (pfpImagePath.isEmpty()) return false;
  auto pfpStore = std::filesystem::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString()) / "profile_pictures";
  std::filesystem::create_directories(pfpStore);
  auto pfpFileName = std::filesystem::path(pfpImagePath.toStdString()).filename();
  auto pfpDestPath = pfpStore / pfpFileName;
  std::filesystem::copy_file(pfpImagePath.toStdString(), pfpDestPath, std::filesystem::copy_options::overwrite_existing);

  ProfileSettings::get()->setValue(STK_PFP, QString::fromStdString(pfpDestPath.string()));
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

    return ss.str();
}

bool createDesktopShortcut(const QString &shortcutName,
                           const QString &launchArgument,
                           const QString &iconPath) {
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

  return QProcess::execute("powershell", {"-NoProfile", "-NonInteractive",
                                          "-Command", psCommand}) == 0;

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
    return false;
  }

  file.write(content.toUtf8());
  file.close();

  // Mark file as executable
  return file.setPermissions(QFile::ExeUser | QFile::ReadUser |
                             QFile::WriteUser | QFile::ReadGroup |
                             QFile::ReadOther);

#else
  return false;
#endif
}
