#pragma once

#include "simulation/electronics/ElectronicsSolver.h"

#include <QObject>
#include <QTimer>

// ---------------------------------------------------------------------------
// SimulationLoop
// Drives all physics solvers at 60 FPS.
// Call setDomain() each frame (or when scene changes) to give the solver
// an up-to-date view of the current components and wires.
// ---------------------------------------------------------------------------
class SimulationLoop final : public QObject {
    Q_OBJECT

public:
    explicit SimulationLoop(QObject* parent = nullptr);

    void start();
    void pause();
    void reset();
    void setSpeed(double multiplier);

    // Supply the solver with the current scene contents.
    // The caller owns the pointers; SimulationLoop never deletes them.
    void setElectronicsDomain(ElectronicsDomain domain);

signals:
    void tickComplete(double simulationTime);

private slots:
    void tick();

private:
    QTimer  timer;
    bool    running       = false;
    double  speed         = 1.0;
    double  simulationTime = 0.0;
    double  fixedFrameDt  = 0.016;  // 16 ms ≈ 60 FPS

    ElectronicsSolver  electronicsSolver;
    ElectronicsDomain  electronicsDomain;
};
