#include <QApplication>
#include <QFontDatabase>
#include "MainWindow.hpp"
#include "utils/RenpyArchive.hpp"
#include <filesystem>
#include <QResource>
#include <pybind11/embed.h>

namespace fs = std::filesystem;
namespace py = pybind11;

int main(int argc, char *argv[]) {
    py::scoped_interpreter guard{};
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("VirtualClub");
    QCoreApplication::setOrganizationName("henrysck075");
    Q_INIT_RESOURCE(resources);

    QFontDatabase::addApplicationFont(QString::fromStdString((fs::path(QCoreApplication::applicationDirPath().toStdString()) 
                            / "assets" / "Quicksand.ttf").string()));

    ModsIndex::loadArchiveReaderModules();
    //LucideIcons::initializeIcons();
    MainWindow window;
    window.show();

    return app.exec();
}


