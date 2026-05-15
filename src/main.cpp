#include "app/MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    // ── Rendering backend ───────────────────────────────────────────────────
    // Qt 6 on Windows uses the raster (software) engine for QWidget-based apps
    // by default — no OpenGL / ANGLE / D3D context is created at startup.
    // Do NOT set Qt::AA_UseDesktopOpenGL: it forces a native OpenGL context
    // which hangs for 30+ minutes on machines without proper OGL 3.3 drivers
    // (most Intel integrated GPUs and all Windows VMs).

    QApplication app(argc, argv);
    QApplication::setApplicationName("Physics Simulation Studio");
    QApplication::setOrganizationName("PhysicsStudio");

    MainWindow window;
    window.show();

    // ── Startup responsiveness fix ──────────────────────────────────────────
    // Pump the OS message queue once before entering the full event loop.
    // Without this, Windows does not receive the initial WM_PAINT/WM_SIZE
    // messages while Qt is still completing one-time initialisation (font
    // cache, backing-store creation, dock widget layout) after show().
    // If that work takes > ~5 s the OS marks the process "Not Responding"
    // in the title bar even before any simulation code has run.
    // processEvents here flushes all pending events and returns — it does
    // NOT start a nested event loop.
    QCoreApplication::processEvents(QEventLoop::ExcludeSocketNotifiers);

    return app.exec();
}
