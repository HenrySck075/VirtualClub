#include <QApplication>
#include <QFontDatabase>
#include "MainWindow.hpp"
#include "utils/ModIndex.hpp"
#include "utils/RenpyArchive.hpp"
#include <QResource>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QFileDialog>

#include "utils/ProfileSettings.hpp"
#include "utils/utils.hpp"
#include "utils/macros.h"

#include <pybind11/embed.h>

#ifdef MVC_DEBUG
#include <cpptrace/from_current_macros.hpp>
#include <cpptrace/from_current.hpp>
#endif

namespace py = pybind11;

int main(int argc, char *argv[]) {
    py::scoped_interpreter guard{};
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("VirtualClub");
    QCoreApplication::setOrganizationName("henrysck075");
    Q_INIT_RESOURCE(resources);

    QFontDatabase::addApplicationFont(":/Quicksand-Bold.ttf");
    QFontDatabase::addApplicationFont(":/Quicksand-Light.ttf");
    QFontDatabase::addApplicationFont(":/Quicksand-Medium.ttf");
    QFontDatabase::addApplicationFont(":/Quicksand-Regular.ttf");
    QFontDatabase::addApplicationFont(":/Quicksand-SemiBold.ttf");

    ModsIndex::loadArchiveReaderModules();
    ModsIndex::loadModsIndex();
    //LucideIcons::initializeIcons();
    //
    QAudioDevice defaultDevice = QMediaDevices::defaultAudioOutput();
    qDebug() << "Default Output Device:" << defaultDevice.description();

    initGlobalSfx();

    auto settings = ProfileSettings::get(); 
    if (!settings->contains("baseGameInstallPath")) {
      askForBasePathChange();
    }

    settings->save();

    MainWindow window;
    window.setWindowIcon(QIcon(":/app-icon.png"));
    window.show();

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


