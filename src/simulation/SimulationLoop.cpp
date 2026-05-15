#include "simulation/SimulationLoop.h"

#include <algorithm>

SimulationLoop::SimulationLoop(QObject* parent)
    : QObject(parent)
{
    // Use a single-shot self-rescheduling pattern instead of a repeating timer.
    // This guarantees the Qt event loop always processes pending events (domain
    // setter calls, pause requests, etc.) between every simulation tick, which
    // allows the main thread to stay completely free of physics work.
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &SimulationLoop::tick);
}

void SimulationLoop::start()
{
    if (running) return;
    running = true;
    m_frameTimer.start();   // begin measuring wall-clock time for 60 FPS pacing
    m_timer.start(0);       // fire immediately, then self-reschedule
}

void SimulationLoop::pause()
{
    running = false;
    m_timer.stop();
}

void SimulationLoop::reset()
{
    pause();
    simulationTime = 0.0;
    emit tickComplete(simulationTime);
}

void SimulationLoop::setSpeed(double multiplier)
{
    speed = std::clamp(multiplier, 0.1, 10.0);
}

void SimulationLoop::setElectronicsDomain(ElectronicsDomain domain)
{
    electronicsDomain = std::move(domain);
}

void SimulationLoop::setMotionDomain(MotionDomain domain)
{
    motionDomain = std::move(domain);
}

void SimulationLoop::setOpticalDomain(OpticalDomain domain)
{
    opticalDomain = std::move(domain);
}

void SimulationLoop::setWaveDomain(WaveDomain domain)
{
    m_waveDomain = std::move(domain);
}

void SimulationLoop::traceOpticsOnce()
{
    if (!opticalDomain.components.isEmpty()) {
        opticalSolver.trace(opticalDomain);
        emit opticsUpdated(opticalDomain.segments);
    }
}

void SimulationLoop::tick()
{
    if (!running)
        return;

    const double dt = fixedFrameDt * speed;
    simulationTime += dt;

    // ── Run solvers ─────────────────────────────────────────────────────────
    // Each solver runs entirely on this worker thread.  UI updates (setPos,
    // item::update, etc.) are posted back to the main thread via
    // QMetaObject::invokeMethod(comp, lambda, Qt::QueuedConnection) inside
    // each solver, so the main thread is never blocked by physics work.

    if (!electronicsDomain.components.isEmpty())
        electronicsSolver.solve(electronicsDomain, dt);

    if (!motionDomain.bodies.isEmpty())
        motionSolver.step(motionDomain, dt);

    if (!opticalDomain.components.isEmpty()) {
        opticalSolver.trace(opticalDomain);
        // Send a copy of the ray list to the overlay on the main thread.
        emit opticsUpdated(opticalDomain.segments);
    }

    const bool hasWaveSources = !m_waveDomain.sources.isEmpty()
                             || !m_waveDomain.planeSources.isEmpty();
    if (hasWaveSources) {
        waveSolver.step(m_waveDomain, dt);
        // Send a copy of the field buffer to the overlay on the main thread.
        emit waveFieldUpdated(m_waveDomain.field,
                              m_waveDomain.cols,
                              m_waveDomain.rows,
                              m_waveDomain.gridSize);
    }

    emit tickComplete(simulationTime);

    if (!running)
        return;

    // ── True 60 FPS pacing ──────────────────────────────────────────────────
    // Measure how long the solvers actually took. If they finished in < 16 ms
    // we wait the remainder so the event loop gets real idle time and the CPU
    // is not pegged at 100 %.  If they took > 16 ms we reschedule immediately
    // (0 ms) to catch up — the simulation will run slightly behind real-time
    // but the UI will still be responsive because control returns to the event
    // loop before every tick, allowing queued domain setters and pause
    // requests to be processed promptly.
    const qint64 elapsed = m_frameTimer.elapsed();
    m_frameTimer.restart();
    const int nextDelay = static_cast<int>(qMax(0LL, 16LL - elapsed));
    m_timer.start(nextDelay);
}
