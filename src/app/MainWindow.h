#pragma once

#include <QMainWindow>
#include <QString>

class CanvasView;
class OpticsOverlay;
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
    void refreshSimulationDomain();   // rebuilds ALL solver domains
    void refreshElectronicsDomain();
    void refreshMotionDomain();
    void refreshOpticsDomain();
    void newModel();
    void openModel();
    void saveModel();
    bool saveModelAs(const QString& path);
    bool loadModelFrom(const QString& path);

    CanvasView*      canvasView      = nullptr;
    OpticsOverlay*   opticsOverlay   = nullptr;
    PropertiesPanel* propertiesPanel = nullptr;
    SimulationLoop*  simulationLoop  = nullptr;
    UndoRedoStack*   undoRedoStack   = nullptr;
    QString currentProjectPath;
};
