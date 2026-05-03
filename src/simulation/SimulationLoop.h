#pragma once

#include <QObject>
#include <QTimer>

class SimulationLoop final : public QObject {
    Q_OBJECT

public:
    explicit SimulationLoop(QObject* parent = nullptr);

    void start();
    void pause();
    void reset();
    void setSpeed(double multiplier);

signals:
    void tickComplete(double simulationTime);

private slots:
    void tick();

private:
    QTimer timer;
    bool running = false;
    double speed = 1.0;
    double simulationTime = 0.0;
    double fixedFrameDt = 0.016;
};

