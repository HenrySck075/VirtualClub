#include <QApplication>
#include <QFontDatabase>
#include "MainWindow.hpp"
#include "utils/ModIndex.hpp"
#include "utils/RenpyArchive.hpp"
#include <QResource>
#include <QMediaDevices>
#include <QAudioDevice>
#include <pybind11/embed.h>
#include <qfiledialog.h>
#include <qsettings.h>

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

    QSettings settings;
    if (!settings.contains("baseGameInstallPath")) {
      auto directory = QFileDialog::getExistingDirectory(nullptr, "Select a base game directory.");
      if (directory != "")
        settings.setValue("baseGameInstallPath", directory);
    }

    MainWindow window;
    window.show();

    return app.exec();
}


