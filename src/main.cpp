#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    // Use Qt's default rendering backend.
    // On Windows 10/11 this is ANGLE (OpenGL ES over Direct3D 11) which:
    //   - Works on ALL Windows hardware (Intel, AMD, Nvidia, virtual machines)
    //   - Is hardware-accelerated via DirectX 11
    //   - Never hangs at startup unlike forcing native desktop OpenGL
    // Do NOT set Qt::AA_UseDesktopOpenGL — that requires real OpenGL drivers
    // which many Windows machines (especially with Intel iGPU) don't have.

    QApplication app(argc, argv);
    QApplication::setApplicationName("Physics Simulation Studio");
    QApplication::setOrganizationName("PhysicsStudio");

    MainWindow window;
    window.show();

    return app.exec();
}
