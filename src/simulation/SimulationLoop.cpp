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

void SimulationLoop::tick()
{
    if (!running) {
        return;
    }

    const double simulationDt = fixedFrameDt * speed;
    simulationTime += simulationDt;

    emit tickComplete(simulationTime);
}

