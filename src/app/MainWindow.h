#pragma once

#include <QMainWindow>
#include <QString>

class CanvasView;
class DataLogger;
class GraphPanel;
class OpticsOverlay;
class PropertiesPanel;
class SimulationLoop;
class UndoRedoStack;
class WaveFieldOverlay;

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
    void refreshWaveDomain();
    void newModel();
    void openModel();
    void saveModel();
    bool saveModelAs(const QString& path);
    bool loadModelFrom(const QString& path);
    void openExample(const QString& resourcePath);

    CanvasView*       canvasView       = nullptr;
    OpticsOverlay*    opticsOverlay    = nullptr;
    WaveFieldOverlay* waveFieldOverlay = nullptr;
    PropertiesPanel*  propertiesPanel  = nullptr;
    SimulationLoop*   simulationLoop   = nullptr;
    UndoRedoStack*    undoRedoStack    = nullptr;
    DataLogger*       dataLogger       = nullptr;
    GraphPanel*       graphPanel       = nullptr;
    QString currentProjectPath;
};
