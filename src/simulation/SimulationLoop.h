#pragma once

#include "simulation/electronics/ElectronicsSolver.h"
#include "simulation/motion/MotionSolver.h"

#include <QObject>
#include <QTimer>

// ---------------------------------------------------------------------------
// SimulationLoop
// Drives all physics solvers at 60 FPS.
// Electronics domain is rebuilt on every scene change (stateless MNA).
// Motion domain persists across ticks to preserve velocity state; rebuild
// only on structural scene changes (components added / deleted).
// ---------------------------------------------------------------------------
class SimulationLoop final : public QObject {
    Q_OBJECT

public:
    explicit SimulationLoop(QObject* parent = nullptr);

    void start();
    void pause();
    void reset();
    void setSpeed(double multiplier);
    bool isRunning() const { return running; }

    // Supply the solvers with current scene contents.
    // The caller owns the component/wire pointers; SimulationLoop never deletes them.
    void setElectronicsDomain(ElectronicsDomain domain);
    void setMotionDomain(MotionDomain domain);

signals:
    void tickComplete(double simulationTime);

private slots:
    void tick();

private:
    QTimer  timer;
    bool    running        = false;
    double  speed          = 1.0;
    double  simulationTime = 0.0;
    double  fixedFrameDt   = 0.016;  // 16 ms ≈ 60 FPS

    ElectronicsSolver  electronicsSolver;
    ElectronicsDomain  electronicsDomain;

    MotionSolver       motionSolver;
    MotionDomain       motionDomain;
};
