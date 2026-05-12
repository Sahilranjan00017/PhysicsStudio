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
    m_timer.start(0);   // fire immediately, then self-reschedule
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

    // Run each solver. After the two heaviest (electronics + wave) we call
    // processEvents so the UI stays responsive even on slow hardware.

    if (!electronicsDomain.components.isEmpty())
        electronicsSolver.solve(electronicsDomain, dt);

    // Let the event loop breathe between heavy solvers.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 2 /*ms*/);

    if (!motionDomain.bodies.isEmpty())
        motionSolver.step(motionDomain, dt);

    if (!opticalDomain.components.isEmpty())
        opticalSolver.trace(opticalDomain);

    if (!m_waveDomain.sources.isEmpty())
        waveSolver.step(m_waveDomain, dt);

    emit tickComplete(simulationTime);

    // Reschedule the next tick. Using 0 ms fires as soon as the event loop
    // is idle, which gives Qt time to process any queued UI events first.
    // The effective simulation rate is limited by solver wall-clock time,
    // which is the correct behaviour for a real-time physics app.
    if (running)
        m_timer.start(0);
}
