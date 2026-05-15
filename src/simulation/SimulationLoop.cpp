#include "simulation/SimulationLoop.h"

#include <QCoreApplication>
#include <algorithm>

SimulationLoop::SimulationLoop(QObject* parent)
    : QObject(parent)
{
    // Use a single-shot self-rescheduling pattern instead of a repeating timer.
    // This guarantees the Qt event loop always processes pending events (user
    // input, repaints, signals) between every simulation tick, which prevents
    // the "Not Responding" freeze on Windows when a solver takes too long.
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
    if (!opticalDomain.components.isEmpty())
        opticalSolver.trace(opticalDomain);
}

void SimulationLoop::tick()
{
    if (!running)
        return;

    const double dt = fixedFrameDt * speed;
    simulationTime += dt;

    // ── Run solvers ─────────────────────────────────────────────────────────
    // After each heavy solver we call processEvents(ExcludeUserInputEvents)
    // so the Qt event loop can flush pending paint / signal events without
    // processing new mouse/key input. This prevents "Not Responding" on
    // Windows even when a single solver tick takes > 16 ms.

    if (!electronicsDomain.components.isEmpty())
        electronicsSolver.solve(electronicsDomain, dt);

    // Breathe after each solver so the Qt event loop can process any pending
    // paint / resize / signal events. ExcludeUserInputEvents means we flush
    // repaints and queued signals without processing mouse / key events, which
    // prevents re-entrant simulation ticks. The 2 ms cap ensures we never
    // spend more than 2 ms in processEvents per call even if there is a backlog.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 2 /*ms*/);

    if (!motionDomain.bodies.isEmpty())
        motionSolver.step(motionDomain, dt);

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 2 /*ms*/);

    if (!opticalDomain.components.isEmpty())
        opticalSolver.trace(opticalDomain);

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 2 /*ms*/);

    if (!m_waveDomain.sources.isEmpty())
        waveSolver.step(m_waveDomain, dt);

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 2 /*ms*/);

    emit tickComplete(simulationTime);

    if (!running)
        return;

    // ── True 60 FPS pacing ──────────────────────────────────────────────────
    // Measure how long the solvers actually took. If they finished in < 16 ms
    // we wait the remainder so the event loop gets real idle time and the CPU
    // is not pegged at 100 %.  If they took > 16 ms we reschedule immediately
    // (0 ms) to catch up — the simulation will run slightly behind real-time
    // but the UI will still be responsive because control returns to the event
    // loop before every tick.
    const qint64 elapsed = m_frameTimer.elapsed();
    m_frameTimer.restart();
    const int nextDelay = static_cast<int>(qMax(0LL, 16LL - elapsed));
    m_timer.start(nextDelay);
}
