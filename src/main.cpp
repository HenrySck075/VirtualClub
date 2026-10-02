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

#include "utils/ProfileSettings.hpp"
#include "utils/ProfileSingleApp.hpp"
#include "utils/macros.h"

#include <pybind11/embed.h>
#include <QTranslator>

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
    py::scoped_interpreter guard{};

    ProfileSingleApp app(argc, argv);


    QTranslator translator;
    if (translator.load(QLocale::system(), "VirtualClub", "_", ":/i18n"))
        app.installTranslator(&translator);

    QCommandLineParser parser;
    QCommandLineOption profileOption(
        QStringList() << "p" << "profile", 
        "Specify profile name", 
        "profile", 
        "default" // Default profile fallback
    );
    parser.addOption(profileOption);
    parser.addHelpOption();
    parser.process(app);

    QString selectedProfile = parser.value(profileOption);
    app.setProfileId(selectedProfile);

    // If this instance is secondary for this profile, pass args to primary and exit
    if (app.isSecondary()) {
        if (app.notifyPrimaryInstance(app.arguments())) {
          return 0; // Terminate secondary instance cleanly
        }
    }
    QCoreApplication::setApplicationName("VirtualClub");
    QCoreApplication::setOrganizationName("henrysck075");
    QCoreApplication::setOrganizationDomain("henrysck.sh");

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

QScrollBar::handle {
  background: %1;
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

    settings->save();

    MainWindow window;
    window.setWindowIcon(QIcon(":/app-icon.png"));
    window.show();

    // Handle messages when a user attempts to re-open the app under this profile
    QObject::connect(&app, &ProfileSingleApp::messageReceivedFromSecondary, [&](const QStringList &args) {
        qDebug() << "Received focus request or arguments from secondary instance:" << args;
        if (window.isMinimized()) {
          window.showNormal();
        }
        window.show();
        window.raise();
        window.activateWindow();
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
}


