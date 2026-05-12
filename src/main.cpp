#include "app/MainWindow.h"

#include <QApplication>
#include <QSurfaceFormat>

int main(int argc, char* argv[])
{
    // ── Hardware OpenGL — must be set before QApplication is constructed ──────
    // On Windows this selects the native OpenGL driver (fast) instead of
    // ANGLE (software emulation), which is the #1 cause of UI lag on Windows.
    QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Default surface format: disable vsync so the render thread never waits
    // for a display blanking interval; the simulation timer controls frame rate.
    QSurfaceFormat fmt;
    fmt.setSwapInterval(0);          // no vsync
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    QApplication::setApplicationName("Physics Simulation Studio");
    QApplication::setOrganizationName("PhysicsStudio");

    MainWindow window;
    window.show();

    return app.exec();
}
