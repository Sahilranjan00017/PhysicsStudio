#pragma once

#include "simulation/electronics/ElectronicsSolver.h"
#include "simulation/motion/MotionSolver.h"
#include "simulation/optics/OpticalSolver.h"
#include "simulation/wave/WaveSolver.h"

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>

#include <atomic>
#include <vector>

// ---------------------------------------------------------------------------
// SimulationLoop
// Drives all physics solvers at 60 FPS on a dedicated QThread.
//
// Lifetime / thread model
// ───────────────────────
// After construction, the caller must move this object to a QThread:
//
//   m_simThread = new QThread(this);
//   simulationLoop->moveToThread(m_simThread);
//   m_simThread->start();
//
// All public methods that are called from the main thread (start, pause,
// reset, setSpeed, domain setters) should be invoked via
// QMetaObject::invokeMethod(..., Qt::QueuedConnection) so they run
// safely on the worker thread.
//
// isRunning() is the only method safe to call from any thread directly,
// because the backing flag is std::atomic<bool>.
//
// Signals emitted from the worker thread
// ───────────────────────────────────────
// tickComplete        → UI updates (status bar, data logger)
// waveFieldUpdated    → WaveFieldOverlay::setField()   (via QueuedConnection)
// opticsUpdated       → OpticsOverlay::setSegments()   (via QueuedConnection)
// ---------------------------------------------------------------------------
class SimulationLoop final : public QObject {
    Q_OBJECT

public:
    explicit SimulationLoop(QObject* parent = nullptr);

    void start();
    void pause();
    void reset();
    void setSpeed(double multiplier);

    // Safe to call from any thread — backed by std::atomic<bool>.
    bool isRunning() const { return running.load(); }

    // Supply the solvers with current scene contents.
    // Must be called on the worker thread (via QueuedConnection) while the
    // simulation is paused so there is no concurrent solver access.
    void setElectronicsDomain(ElectronicsDomain domain);
    void setMotionDomain(MotionDomain domain);
    void setOpticalDomain(OpticalDomain domain);
    void setWaveDomain(WaveDomain domain);

    // Runs one optical trace pass and emits opticsUpdated (used for static
    // preview when paused). Must be called on the worker thread.
    void traceOpticsOnce();

signals:
    // Emitted from the worker thread after every physics tick.
    void tickComplete(double simulationTime);

    // Emitted after each wave-solver tick with a copy of the field buffer.
    // Connected to WaveFieldOverlay::setField() via Qt::QueuedConnection.
    void waveFieldUpdated(std::vector<float> field, int cols, int rows, double gridSize);

    // Emitted after each optics-solver tick (and after traceOpticsOnce) with
    // a copy of the ray-segment list.
    // Connected to OpticsOverlay::setSegments() via Qt::QueuedConnection.
    void opticsUpdated(QList<OpticalSegment> segments);

private slots:
    void tick();

private:
    QTimer        m_timer;
    QElapsedTimer m_frameTimer;     // measures actual wall-clock time per tick

    std::atomic<bool> running{false};
    double  speed          = 1.0;
    double  simulationTime = 0.0;
    double  fixedFrameDt   = 0.016;  // 16 ms ≈ 60 FPS

    ElectronicsSolver  electronicsSolver;
    ElectronicsDomain  electronicsDomain;

    MotionSolver       motionSolver;
    MotionDomain       motionDomain;

    OpticalSolver      opticalSolver;
    OpticalDomain      opticalDomain;

    WaveSolver         waveSolver;
    WaveDomain         m_waveDomain;
};
