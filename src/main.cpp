#include <QApplication>
#include <QFontDatabase>
#include "MainWindow.hpp"
#include "consts.hpp"
#include "screens/UserSetupDialog.hpp"
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
#include <memory>

#include "utils/ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include "utils/ModIndex.hpp"
#include "utils/SessionManager.hpp"
#include "utils/i18n.hpp"
#include "utils/macros.h"

#include <pybind11/embed.h>
#include <QTranslator>
#include <pylifecycle.h>
#include <qsettings.h>

#ifdef MVC_DEBUG
#include <cpptrace/from_current_macros.hpp>
#include <cpptrace/from_current.hpp>
#endif

namespace py = pybind11;

QString turkye(const QColor& color) {
  return QString("rgb(%1,%2,%3)")
    .arg(color.red())
    .arg(color.green())
    .arg(color.blue())
  ;
}

int main(int argc, char *argv[]) {

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
qDebug() << "Supported image formats:" << QImageReader::supportedImageFormats();
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
    qDebug() << "Default Output Device:" << defaultDevice.description();

    initGlobalSfx();

    app.setStyleSheet(QString(R"(
QLabel {
  font-family: Quicksand, Segoe UI;
  color: %1;
}

QLineEdit {
  border: 0px;
  border-bottom: 2px solid %2;
  font-family: Quicksand, Segoe UI;
  padding: 4px;
}

QScrollBar {
  background-color: transparent;
}
QScrollBar:vertical {
  width: 14px;
  margin: 4px 7px 4px 0px;
}
QScrollBar:horizontal {
  height: 14px;
  margin: 0px 4px 7px 4px;
}
QScrollBar::handle {
  background-color: %1;
}
QScrollBar::add-line, QScrollBar::sub-line {
  width: 0px;
  height: 0px;
}

QComboBox {
  background-color: transparent;
  border: 2px solid %1;
  border-radius: 4px;
  color: %2;
  padding: 4px 8px;
}

)").arg(turkye(c_primaryColor)).arg(turkye(c_secondaryColor)));


    showUserSetupDialogIfNeeded();

    auto settings = ProfileSettings::get(); 

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
    if (!initialModId.isEmpty()) {
        launchMod(initialModId);
    }

    std::unique_ptr<MainWindow> window = std::make_unique<MainWindow>();
    window->setWindowIcon(QIcon(":/app-icon.png"));
    if (initialModId.isEmpty()) {
        window->show();
    }

    // Handle messages when a user attempts to re-open the app under this profile
    QObject::connect(&app, &ProfileSingleApp::messageReceivedFromSecondary, [&](const QStringList &args) {
        qDebug() << "Received focus request or arguments from secondary instance:" << args;
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

#ifdef MVC_DEBUG
    CPPTRACE_TRY {
#endif
    return app.exec();
#ifdef MVC_DEBUG
    } CPPTRACE_CATCH (std::exception& e) {
      qDebug() << "Exception:" << e.what();
      cpptrace::from_current_exception().print();
    }
#endif

    py::finalize_interpreter();
}
