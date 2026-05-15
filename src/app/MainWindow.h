#pragma once

#include <QMainWindow>
#include <QString>
#include <QTimer>

class CanvasView;
class DataLogger;
class GraphPanel;
class OpticsOverlay;
class PropertiesPanel;
class SimulationLoop;
class UndoRedoStack;
class WaveFieldOverlay;
class QCloseEvent;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

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
    void updateWindowTitle();
    void newModel();
    void openModel();
    void saveModel();
    void saveModelAsDialog();         // prompts for path, then saves
    bool saveModelAs(const QString& path);
    bool loadModelFrom(const QString& path);
    void openExample(const QString& resourcePath);
    void fitView();

    CanvasView*       canvasView       = nullptr;
    OpticsOverlay*    opticsOverlay    = nullptr;
    WaveFieldOverlay* waveFieldOverlay = nullptr;
    PropertiesPanel*  propertiesPanel  = nullptr;
    SimulationLoop*   simulationLoop   = nullptr;
    UndoRedoStack*    undoRedoStack    = nullptr;
    DataLogger*       dataLogger       = nullptr;
    GraphPanel*       graphPanel       = nullptr;
    QTimer            m_renderTimer;           // 30 FPS repaint — decoupled from physics
    QTimer            m_refreshTimer;          // debounce for scene-changed → domain rebuild
    QString           currentProjectPath;
    bool              m_dirty          = false;
};
