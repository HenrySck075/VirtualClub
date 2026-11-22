#include <QApplication>
#include <QFontDatabase>
#include "MainWindow.hpp"
#include "consts.hpp"
#include "screens/UserSetupDialog.hpp"
#include "ui/MESWidgets.hpp"
#include "utils/BackgroundLoader.hpp"
#include "utils/RenpyArchive.hpp"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QFileDialog>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QImageReader>
#include <QSet>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <set>

#include "utils/ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include "utils/ModIndex.hpp"
#include "utils/SessionManager.hpp"
#include "utils/i18n.hpp"
#include "utils/macros.h"
#include "utils/utils.hpp"
#include "utils/AutoUpdater.hpp"

#include <pybind11/embed.h>
#include <QTranslator>
#include <pylifecycle.h>
#include <qsettings.h>
#include <QDateTime>

#ifdef WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef MVC_DEBUG
#include <cpptrace/from_current_macros.hpp>
#include <cpptrace/from_current.hpp>
#endif

namespace py = pybind11;

namespace fs = std::filesystem;

static bool mergeDirectoryContents(const fs::path& source, const fs::path& destination) {
  std::error_code error;
  fs::create_directories(destination, error);
  if (error) {
    qWarning() << "Could not create migration directory:"
           << QString::fromStdString(destination.string())
           << QString::fromStdString(error.message());
    return false;
  }

  bool succeeded = true;
  for (const auto& entry : fs::directory_iterator(source, error)) {
    if (error) {
      qWarning() << "Could not read migration directory:"
             << QString::fromStdString(source.string())
             << QString::fromStdString(error.message());
      return false;
    }

    const auto sourcePath = entry.path();
    const auto destinationPath = destination / sourcePath.filename();
    const bool isDirectory = entry.is_directory(error);
    if (error) {
      qWarning() << "Could not inspect migration entry:"
             << QString::fromStdString(sourcePath.string())
             << QString::fromStdString(error.message());
      succeeded = false;
      continue;
    }

    if (isDirectory) {
      if (!mergeDirectoryContents(sourcePath, destinationPath)) {
        succeeded = false;
      }
      continue;
    }

    fs::remove_all(destinationPath, error);
    if (error) {
      qWarning() << "Could not replace migration entry:"
             << QString::fromStdString(destinationPath.string())
             << QString::fromStdString(error.message());
      succeeded = false;
      continue;
    }

    fs::copy(sourcePath, destinationPath, fs::copy_options::overwrite_existing, error);
    if (error) {
      qWarning() << "Could not migrate database entry:"
             << QString::fromStdString(sourcePath.string())
             << QString::fromStdString(error.message());
      succeeded = false;
      continue;
    }

    fs::remove(sourcePath, error);
    if (error) {
      qWarning() << "Could not remove migrated database entry:"
             << QString::fromStdString(sourcePath.string())
             << QString::fromStdString(error.message());
      succeeded = false;
    }
  }

  if (succeeded) {
    fs::remove(source, error);
    if (error) {
      qWarning() << "Could not remove migrated database folder:"
             << QString::fromStdString(source.string())
             << QString::fromStdString(error.message());
      return false;
    }
  }
  return succeeded;
}

// a rule im imposing on compat logic is that these thing will be here for 5 versions max

// <=v0.1.5 -> v0.2.0
static void processCompatibilityMigrations() {
  QSettings settings;
  if (settings.value(GSTK_SESSION_DATA_MIGRATED, false).toBool()) {
    return;
  }

  const auto storagePath = fs::path(settings.value(GSTK_STORAGE_PATH).toString().toStdString());
  const auto sessionDataPath = storagePath / "session_data";
  std::error_code error;
  fs::create_directories(sessionDataPath, error);
  if (error) {
    qWarning() << "Could not create session data directory:"
           << QString::fromStdString(sessionDataPath.string())
           << QString::fromStdString(error.message());
    return;
  }

  std::vector<fs::path> roots = {
    fs::path(settings.value(GSTK_STORAGE_PATH).toString().toStdString()),
    storagePath,
    fs::path(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString())
  };
  std::set<fs::path> scannedRoots;
  bool migrationSucceeded = true;

  for (const auto& root : roots) {
    if (root.empty() || !fs::exists(root) || !scannedRoots.insert(root).second) {
      continue;
    }

    fs::recursive_directory_iterator iterator(
      root, fs::directory_options::skip_permission_denied, error);
    const fs::recursive_directory_iterator end;
    while (iterator != end) {
      if (error) {
        qWarning() << "Could not scan legacy database folders:"
               << QString::fromStdString(root.string())
               << QString::fromStdString(error.message());
        error.clear();
        iterator.increment(error);
        continue;
      }

      const auto current = iterator->path();
      if (current == sessionDataPath) {
        iterator.disable_recursion_pending();
        iterator.increment(error);
        continue;
      }

      if (iterator->is_directory(error) && current.filename() == ".vclubmgr") {
        if (!mergeDirectoryContents(current, sessionDataPath)) {
          qWarning() << "Legacy database migration was incomplete for:"
                 << QString::fromStdString(current.string());
          migrationSucceeded = false;
        }
        iterator.disable_recursion_pending();
      }
      iterator.increment(error);
    }
  }

  if (migrationSucceeded) {
    settings.setValue(GSTK_SESSION_DATA_MIGRATED, true);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
      qWarning() << "Could not save legacy database migration state.";
    }
  }
}


int main(int argc, char *argv[]) {

#ifdef WIN32
  // DLLs for optional/runtime-loaded components may be kept in a bin directory.
  // Add it to the process DLL search path before Pybind/Python and WinFsp are
  // initialized. Linked DLLs (Qt, Python, LibArchive) still need to be beside
  // the executable because the OS loader resolves import-table dependencies
  // before main().
  wchar_t executablePath[MAX_PATH];
  if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) != 0) {
    std::wstring executable(executablePath);
    const auto separator = executable.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
      const std::wstring binDir = executable.substr(0, separator) + L"\\bin";
      SetDllDirectoryW(binDir.c_str());
    }
  }
#endif

  ProfileSingleApp app(argc, argv);

  QCoreApplication::setApplicationName("VirtualClub");
  QCoreApplication::setOrganizationName("henrysck075");
  QCoreApplication::setOrganizationDomain("henrysck.sh");


  QCommandLineParser parser;
  QCommandLineOption profileOption(
      QStringList() << "p" << "profile", 
      "Specify profile name", 
      "profile", 
      "" // Default profile fallback
  );
  parser.addOption(profileOption);
  QCommandLineOption modOption(
      "mod",
      "Launch a mod in the background (modid is the launcher's internal ID assigned for the mod)",
      "modid"
  );
  parser.addOption(modOption);
  parser.addHelpOption();
  parser.process(app);

  QSettings qsettings;

  QString selectedProfile = parser.value(profileOption).trimmed();
  if (selectedProfile.isEmpty()) {
    selectedProfile = qsettings.value("lastUsedProfile","").toString();
    // this is to skip querying the profiles list if selectedProfile is already set
    if (selectedProfile.isEmpty()) {
      selectedProfile = ProfileSettings::list().value(0, "default"); 
    }
  }
  app.setProfileId(selectedProfile);

  // If this instance is secondary for this profile, pass args to primary and exit
  if (app.isSecondary()) {
      if (app.notifyPrimaryInstance(app.arguments())) {
        return 0; // Terminate secondary instance cleanly
      }
  }

  if (!qsettings.contains(GSTK_STORAGE_PATH)) {
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/storage";
    qsettings.setValue(GSTK_STORAGE_PATH, defaultPath);
  }
  // create storage folder 
  if (!QDir(qsettings.value(GSTK_STORAGE_PATH).toString()).exists()) {
    QDir().mkpath(qsettings.value(GSTK_STORAGE_PATH).toString());
  }
  processCompatibilityMigrations();

  PyConfig config;
  PyConfig_InitPythonConfig(&config);

  auto appDir = QCoreApplication::applicationDirPath();
  if (QDir(appDir + "/lib").exists() || QDir(appDir + "/Lib").exists()) {
    if (PyStatus_Exception(PyConfig_SetString(&config, &config.home, appDir.toStdWString().c_str()))) {
      throw std::runtime_error("Failed to set Python home directory");
    };
  }

  py::initialize_interpreter(&config);

  QFontDatabase::addApplicationFont(":/Quicksand-Bold.ttf");
  QFontDatabase::addApplicationFont(":/Quicksand-Light.ttf");
  QFontDatabase::addApplicationFont(":/Quicksand-Medium.ttf");
  QFontDatabase::addApplicationFont(":/Quicksand-Regular.ttf");
  QFontDatabase::addApplicationFont(":/Quicksand-SemiBold.ttf");

  ArchiveReader::loadArchiveReaderModules();
  //LucideIcons::initializeIcons();
  //
  QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
  

  initGlobalSfx();

  setupDesignStylesheet();


  showUserSetupDialogIfNeeded();

  auto settings = ProfileSettings::get(); 

  if (!settings->contains(STK_BASEGAMEID) && settings->contains(STK_BASEPATH)) {
    const auto baseGamePath =
      fs::path(settings->value(STK_BASEPATH).toString().toStdString()) / "game";
    try {
      settings->setValue(
        STK_BASEGAMEID,
        QString::fromStdString(ArchiveReader::readBuildId(baseGamePath))
      );
    } catch (const std::exception& error) {
      qWarning() << "Could not determine base game build ID:"
                 << error.what();
    }
  }

  if (!settings->contains(STK_BGPACK)) {
    auto packs = BackgroundLoader::getPacks();
    if (!packs.isEmpty()) {
      settings->setValue(STK_BGPACK, packs.first());
    }
  }

  if (!settings->contains(STK_LANGUAGE)) {
    auto systemLocale = QLocale::system();
    QLocale targetLocale(QLocale::Language::English);
    if (getAvailableLocales().contains(systemLocale)) 
      targetLocale = systemLocale;
    
    settings->setValue(STK_LANGUAGE, targetLocale.name());
  }

  settings->save();

  setLanguage(QLocale(settings->value(STK_LANGUAGE).toString()));
  setupReTLHandler();

  QSet<QString> launchedMods;
  auto launchMod = [&launchedMods](const QString& modId) {
      const auto normalizedModId = modId.trimmed();
      if (normalizedModId.isEmpty() || launchedMods.contains(normalizedModId)) {
          return;
      }

      const auto& mods = ModsIndex::get()->getMods();
      const auto mod = std::find_if(mods.begin(), mods.end(), [&normalizedModId](const ModsIndex::Mod& candidate) {
          return QString::fromStdString(candidate.id) == normalizedModId;
      });
      if (mod == mods.end()) {
          qWarning() << "Could not launch mod; no installed mod has ID:" << normalizedModId;
          return;
      }

      launchedMods.insert(normalizedModId);
      try {
          auto modToLaunch = *mod;
          SessionManager::launch(
              modToLaunch,
              []() {},
              [normalizedModId, &launchedMods]() {
                  launchedMods.remove(normalizedModId);
              }
          );
      } catch (const std::exception& exception) {
          launchedMods.remove(normalizedModId);
          qWarning() << "Could not launch mod" << normalizedModId << ":" << exception.what();
      }
  };

  const auto initialModId = parser.value(modOption);

  std::unique_ptr<MainWindow> window = std::make_unique<MainWindow>();
  window->setWindowIcon(QIcon(":/app-icon.png"));
  if (initialModId.isEmpty()) {
      window->show();
  }

  if (!initialModId.isEmpty()) {
      launchMod(initialModId);
  }

  auto* autoUpdater = new AutoUpdater(window.get());
  QObject::connect(autoUpdater, &AutoUpdater::updateAvailable, window.get(),
      [](const nlohmann::json& manifest) {
          if (auto* mainWindow = getMainWindow()) {
             mainWindow->showUpdateAvailable(manifest);
          }
      });
  QObject::connect(autoUpdater, &AutoUpdater::checkFailed, window.get(),
      [](const QString& error) {
          qWarning() << "Update check failed:" << error;
      });

  const QString frequency = qsettings.value(GSTK_UPDATE_FREQUENCY, "daily").toString();
  const QString channel =
      qsettings.value(GSTK_UPDATE_CHANNEL, QStringLiteral(MVC_CHANNEL)).toString();
  const QDateTime lastCheck = qsettings.value(GSTK_UPDATE_LAST_CHECK).toDateTime();
  bool checkDue = frequency == "startup";
  if (frequency == "daily") checkDue = !lastCheck.isValid() || lastCheck.addDays(1) <= QDateTime::currentDateTime();
  if (frequency == "weekly") checkDue = !lastCheck.isValid() || lastCheck.addDays(7) <= QDateTime::currentDateTime();
  if (checkDue && initialModId.isEmpty() && channel != "local") {
      qsettings.setValue(GSTK_UPDATE_LAST_CHECK, QDateTime::currentDateTime());
      autoUpdater->checkForUpdate();
  }


  // Handle messages when a user attempts to re-open the app under this profile
  QObject::connect(&app, &ProfileSingleApp::messageReceivedFromSecondary, [&](const QStringList &args) {
      
      QCommandLineParser secondaryParser;
      secondaryParser.addOption(modOption);
      secondaryParser.process(args);

      const auto secondaryModId = secondaryParser.value(modOption);
      if (!secondaryModId.isEmpty()) {
          launchMod(secondaryModId);
      }

      else if (window) {
          if (window->isMinimized()) {
            window->showNormal();
          }
          window->show();
          window->raise();
          window->activateWindow();
      }
  });
  
  QObject::connect(&app, &QCoreApplication::aboutToQuit, [selectedProfile]() {
      QSettings().setValue("lastUsedProfile", selectedProfile);
  });

  int exitcodemethink = 0;
#ifdef MVC_DEBUG
  CPPTRACE_TRY 
#endif
  {
  // Keep the embedded interpreter available to worker threads while Qt owns
  // the event loop. Python work acquires/releases the GIL explicitly.
  py::gil_scoped_release gilRelease;
  std::cout << "==== Welcome to VirtualClub " MVC_VERSION " ====" << std::endl;
  exitcodemethink = app.exec();
  }
#ifdef MVC_DEBUG
  CPPTRACE_CATCH (std::exception& e) {  
    cpptrace::from_current_exception().print();
  }
#endif

  py::finalize_interpreter();

  return exitcodemethink;
}
