#pragma once

#include <QMainWindow>

class CanvasView;
class SimulationLoop;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildMenus();
    void buildToolbar();
    void buildDocks();

    CanvasView* canvasView = nullptr;
    SimulationLoop* simulationLoop = nullptr;
};

