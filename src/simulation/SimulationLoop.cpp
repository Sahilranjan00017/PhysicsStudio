#include "simulation/SimulationLoop.h"

#include <algorithm>

SimulationLoop::SimulationLoop(QObject* parent)
    : QObject(parent)
{
    timer.setInterval(16);
    connect(&timer, &QTimer::timeout, this, &SimulationLoop::tick);
}

void SimulationLoop::start()
{
    running = true;
    timer.start();
}

void SimulationLoop::pause()
{
    running = false;
    timer.stop();
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

void SimulationLoop::tick()
{
    if (!running)
        return;

    const double dt = fixedFrameDt * speed;
    simulationTime += dt;

    // --- Electronics (stateless MNA — recomputes every tick) ---
    if (!electronicsDomain.components.isEmpty())
        electronicsSolver.solve(electronicsDomain, dt);

    // --- Motion (stateful — domain persists, bodies integrate over time) ---
    if (!motionDomain.bodies.isEmpty())
        motionSolver.step(motionDomain, dt);

    // --- Optics / Waves: solvers added in Phase 4 ---

    emit tickComplete(simulationTime);
}
