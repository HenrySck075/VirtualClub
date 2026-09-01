#include <QApplication>
#include <QFontDatabase>
#include "MainWindow.hpp"
#include <filesystem>
#include <QResource>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("VirtualClub");
    QCoreApplication::setOrganizationName("henrysck075");
    Q_INIT_RESOURCE(resources);

    QFontDatabase::addApplicationFont(QString::fromStdString((fs::path(QCoreApplication::applicationDirPath().toStdString()) 
                            / "assets" / "Quicksand.ttf").string()));

    //LucideIcons::initializeIcons();
    MainWindow window;
    window.show();

    return app.exec();
}


