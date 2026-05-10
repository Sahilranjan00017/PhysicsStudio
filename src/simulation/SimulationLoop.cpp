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

void SimulationLoop::tick()
{
    if (!running)
        return;

    const double dt = fixedFrameDt * speed;
    simulationTime += dt;

    // --- Electronics ---
    if (!electronicsDomain.components.isEmpty())
        electronicsSolver.solve(electronicsDomain, dt);

    // --- Motion / Optics / Waves: solvers added in Phase 3-4 ---

    emit tickComplete(simulationTime);
}
