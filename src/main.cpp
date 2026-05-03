#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Physics Simulation Studio");
    QApplication::setOrganizationName("PhysicsStudio");

    MainWindow window;
    window.show();

    return app.exec();
}

