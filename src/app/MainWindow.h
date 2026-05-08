#pragma once

#include <QMainWindow>

class CanvasView;
class PropertiesPanel;
class SimulationLoop;
class UndoRedoStack;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void buildMenus();
    void buildToolbar();
    void buildDocks();
    void connectCanvasSelection();

    CanvasView* canvasView = nullptr;
    PropertiesPanel* propertiesPanel = nullptr;
    SimulationLoop* simulationLoop = nullptr;
    UndoRedoStack* undoRedoStack = nullptr;
};
