#include "app/MainWindow.h"

#include "canvas/CanvasView.h"
#include "components/BaseComponent.h"
#include "components/ComponentRegistry.h"
#include "components/ConnectionPad.h"
#include "components/Wire.h"
#include "interaction/UndoRedoStack.h"
#include "persistence/ProjectDocument.h"
#include "canvas/OpticsOverlay.h"
#include "canvas/WaveFieldOverlay.h"
#include "simulation/SimulationLoop.h"
#include "simulation/electronics/ElectronicsSolver.h"
#include "simulation/motion/MotionSolver.h"
#include "simulation/optics/OpticalSolver.h"
#include "simulation/wave/WaveSolver.h"
#include "ui/contentspanel/ContentsPanel.h"
#include "ui/graph/DataLogger.h"
#include "ui/graph/GraphPanel.h"
#include "ui/partspanel/PartsPanel.h"
#include "ui/properties/PropertiesPanel.h"

#include <cmath>

#include <QAction>
#include <QCloseEvent>
#include <QMetaObject>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QIODevice>
#include <QJsonDocument>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointF>
#include <QRectF>
#include <QShortcut>
#include <QStandardPaths>
#include <QStatusBar>
#include <QString>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      canvasView(new CanvasView(this)),
      propertiesPanel(new PropertiesPanel(this)),
      simulationLoop(new SimulationLoop()),   // no parent — required before moveToThread
      undoRedoStack(new UndoRedoStack(this)),
      dataLogger(new DataLogger(this))
{
    registerCoreComponents(ComponentRegistry::instance());
    canvasView->setUndoRedoStack(undoRedoStack);
    propertiesPanel->setUndoRedoStack(undoRedoStack);

    setWindowTitle("Physics Simulation Studio");
    resize(1280, 800);
    setCentralWidget(canvasView);

    buildMenus();
    buildToolbar();
    buildDocks();
    connectCanvasSelection();
    statusBar()->showMessage("Ready");
    updateWindowTitle();

    // ── Simulation worker thread ─────────────────────────────────────────────
    // Moving SimulationLoop to its own QThread means all physics computation
    // (MNA, Euler integration, ray tracing, wave superposition) runs completely
    // off the main thread.  The UI thread is free to handle user input and paint
    // at full frame rate regardless of how heavy the current simulation is.
    m_simThread = new QThread(this);
    simulationLoop->moveToThread(m_simThread);
    m_simThread->start();

    // Create the optics ray-path overlay (z=50) and wave field overlay (z=40).
    // These now own their data buffers; the simulation worker sends updates via
    // signals so there is no shared state between threads.
    opticsOverlay = new OpticsOverlay();
    opticsOverlay->setZValue(50.0);
    canvasView->graphicsScene()->addItem(opticsOverlay);

    waveFieldOverlay = new WaveFieldOverlay();
    waveFieldOverlay->setZValue(40.0);
    canvasView->graphicsScene()->addItem(waveFieldOverlay);

    // Wire simulation signals to overlay update slots.
    // Qt::AutoConnection across threads → Qt::QueuedConnection automatically,
    // so the lambda runs on the main thread even though the signal is emitted
    // from the worker thread.
    connect(simulationLoop, &SimulationLoop::waveFieldUpdated,
            this, [this](std::vector<float> field, int cols, int rows, double gridSize) {
                if (waveFieldOverlay)
                    waveFieldOverlay->setField(std::move(field), cols, rows, gridSize);
            });
    connect(simulationLoop, &SimulationLoop::opticsUpdated,
            this, [this](QList<OpticalSegment> segments) {
                if (opticsOverlay)
                    opticsOverlay->setSegments(std::move(segments));
            });

    // Space-bar toggles play / pause.
    // isRunning() is safe from any thread (std::atomic<bool>).
    // pause/start must be queued to the worker thread via invokeMethod.
    auto* spacebar = new QShortcut(Qt::Key_Space, this);
    connect(spacebar, &QShortcut::activated, this, [this] {
        if (simulationLoop->isRunning())
            QMetaObject::invokeMethod(simulationLoop, [this]() { simulationLoop->pause(); });
        else
            QMetaObject::invokeMethod(simulationLoop, [this]() { simulationLoop->start(); });
    });

    connect(canvasView, &CanvasView::componentPlaced, this, [this](const QString& typeId, const QPointF& position) {
        statusBar()->showMessage(
            QString("Placed %1 at (%2, %3)")
                .arg(typeId)
                .arg(position.x(), 0, 'f', 0)
                .arg(position.y(), 0, 'f', 0),
            3000);
        refreshSimulationDomain();
        m_dirty = true;
        updateWindowTitle();
    });

    // ── Scene-change → domain rebuild (debounced) ───────────────────────────
    // QGraphicsScene::changed() is emitted whenever *any* item calls update(),
    // including the overlay items driven by m_renderTimer at 30 FPS.
    // Connecting directly to refreshSimulationDomain() would therefore call
    // it 30× per second on every idle frame — burning the UI thread for no
    // reason and causing "Not Responding" on slower Windows hardware.
    //
    // Fix: m_refreshTimer is a one-shot timer set to 150 ms. changed() restarts
    // it each time it fires; refreshSimulationDomain() only executes after the
    // scene has been *quiet* for 150 ms, collapsing any burst of rapid updates
    // (drag-move, property change) into a single rebuild.
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(150);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        if (!simulationLoop->isRunning())
            refreshSimulationDomain();
    });

    connect(canvasView->graphicsScene(), &QGraphicsScene::changed,
            this, [this](const QList<QRectF>&) {
                if (!simulationLoop->isRunning()) {
                    m_refreshTimer.start();   // restarts the 150 ms countdown
                    m_dirty = true;
                    updateWindowTitle();
                }
            });

    // Simulation ticks only update data — rendering is handled by m_renderTimer.
    // Decoupling physics from rendering means the UI stays smooth even when
    // solvers are slow: the render timer fires at 30 FPS regardless.
    connect(simulationLoop, &SimulationLoop::tickComplete,
            this, [this](double simTime) {
                dataLogger->onTick(simTime);
                statusBar()->showMessage(QString("t = %1 s").arg(simTime, 0, 'f', 2));
            });

    // ── Independent 30 FPS render timer ─────────────────────────────────────
    // Drives overlay repaints at 30 FPS decoupled from the physics tick rate.
    // Only the overlays (optics rays, wave field) need continuous repainting
    // while the simulation is running. The canvas viewport itself is repainted
    // by Qt automatically whenever scene items change (e.g. components moving),
    // so we do NOT call viewport()->update() unconditionally here — that would
    // force a full-scene repaint 30× per second even on a static paused canvas,
    // keeping the UI thread busy for no visual gain.
    m_renderTimer.setInterval(33);   // ~30 FPS
    m_renderTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_renderTimer, &QTimer::timeout, this, [this]() {
        if (simulationLoop->isRunning()) {
            // Only force overlay repaints while the simulation is live.
            if (opticsOverlay)    opticsOverlay->update();
            if (waveFieldOverlay) waveFieldOverlay->update();
            // The canvas items (balls, blocks, etc.) call setPos() each tick
            // which automatically triggers QGraphicsView to repaint — no
            // extra viewport()->update() needed here.
        }
    });
    m_renderTimer.start();
}

MainWindow::~MainWindow()
{
    // Stop the simulation worker thread cleanly before Qt begins destroying
    // child objects.  quit() posts a quit event to the worker event loop;
    // wait() blocks until the thread exits.  After wait() returns, the worker
    // thread is no longer accessing simulationLoop, so it is safe to delete.
    if (m_simThread) {
        m_simThread->quit();
        m_simThread->wait();
    }
    delete simulationLoop;
    simulationLoop = nullptr;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_dirty) {
        const auto btn = QMessageBox::question(
            this,
            "Unsaved Changes",
            "The current model has unsaved changes.\n"
            "Do you want to save before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if (btn == QMessageBox::Save) {
            saveModel();
            if (m_dirty) {        // user cancelled the save dialog
                event->ignore();
                return;
            }
        } else if (btn == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::fitView()
{
    const QRectF bounds = canvasView->graphicsScene()->itemsBoundingRect();
    const QRectF target = bounds.isEmpty()
        ? canvasView->graphicsScene()->sceneRect()
        : bounds.adjusted(-60.0, -60.0, 60.0, 60.0);
    canvasView->fitInView(target, Qt::KeepAspectRatio);
}

void MainWindow::buildMenus()
{
    // ── File ─────────────────────────────────────────────────────────────────
    auto* fileMenu  = menuBar()->addMenu("&File");
    auto* newAction  = fileMenu->addAction("New Model",  this, &MainWindow::newModel);
    auto* openAction = fileMenu->addAction("Open Model", this, &MainWindow::openModel);
    auto* saveAction = fileMenu->addAction("Save",       this, &MainWindow::saveModel);
    auto* saveAsAction = fileMenu->addAction("Save As…", this, &MainWindow::saveModelAsDialog);
    newAction->setShortcut(QKeySequence::New);
    openAction->setShortcut(QKeySequence::Open);
    saveAction->setShortcut(QKeySequence::Save);
    saveAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    fileMenu->addSeparator();

    auto* examplesMenu = fileMenu->addMenu("Open Example");

    // Electronics examples
    examplesMenu->addAction("Electronics — Voltage Divider", this, [this] {
        openExample(":/examples/electronics_divider.pss");
    });
    examplesMenu->addAction("Electronics — RC Filter", this, [this] {
        openExample(":/examples/electronics_rc_filter.pss");
    });
    examplesMenu->addAction("Electronics — NPN Transistor Switch", this, [this] {
        openExample(":/examples/electronics_transistor.pss");
    });

    // Motion examples
    examplesMenu->addSeparator();
    examplesMenu->addAction("Motion — Bouncing Balls", this, [this] {
        openExample(":/examples/motion_bounce.pss");
    });
    examplesMenu->addAction("Motion — Pendulum", this, [this] {
        openExample(":/examples/motion_pendulum.pss");
    });

    // Optics examples
    examplesMenu->addSeparator();
    examplesMenu->addAction("Optics — Lens Focusing", this, [this] {
        openExample(":/examples/optics_lens.pss");
    });

    // Wave examples
    examplesMenu->addSeparator();
    examplesMenu->addAction("Waves — Two-Source Interference", this, [this] {
        openExample(":/examples/waves_interference.pss");
    });
    examplesMenu->addAction("Waves — Diffraction & Ripple", this, [this] {
        openExample(":/examples/waves_ripple.pss");
    });

    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    // ── Edit ─────────────────────────────────────────────────────────────────
    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoRedoStack->qtStack()->createUndoAction(this, "Undo"));
    editMenu->addAction(undoRedoStack->qtStack()->createRedoAction(this, "Redo"));
    editMenu->addSeparator();
    auto* deleteAction = editMenu->addAction("Delete Selected",
                                             canvasView,
                                             &CanvasView::deleteSelectedComponents);
    deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));

    // ── View ─────────────────────────────────────────────────────────────────
    auto* viewMenu  = menuBar()->addMenu("&View");
    auto* fitAction = viewMenu->addAction("Fit in View", this, &MainWindow::fitView);
    fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    viewMenu->addSeparator();
    auto* zoomInAction  = viewMenu->addAction("Zoom In",  this,
        [this] { canvasView->scale(1.25, 1.25); });
    auto* zoomOutAction = viewMenu->addAction("Zoom Out", this,
        [this] { canvasView->scale(0.80, 0.80); });
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);

    // ── Simulation ────────────────────────────────────────────────────────────
    auto* simulationMenu = menuBar()->addMenu("&Simulation");
    auto* playAction  = simulationMenu->addAction("Play",  simulationLoop, &SimulationLoop::start);
    auto* pauseAction = simulationMenu->addAction("Pause", simulationLoop, &SimulationLoop::pause);
    simulationMenu->addAction("Reset", this, [this] {
        QMetaObject::invokeMethod(simulationLoop, [this]() { simulationLoop->reset(); });
        dataLogger->clearData();
        statusBar()->showMessage("Simulation reset", 2000);
    });
    playAction->setShortcut(Qt::Key_F5);
    pauseAction->setShortcut(Qt::Key_F6);

    // ── Help ─────────────────────────────────────────────────────────────────
    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("About Physics Studio", this, [this] {
        QMessageBox::about(this, "About Physics Studio",
            "<h3>Physics Simulation Studio</h3>"
            "<p><b>Version 1.0.6</b></p>"
            "<p>An educational multi-domain physics simulator.</p>"
            "<p>Supported domains &amp; components:</p>"
            "<ul>"
            "<li><b>Electronics</b> — MNA solver: resistors, capacitors, inductors, "
            "voltage/current sources, diodes, transistors, op-amps</li>"
            "<li><b>Motion</b> — Symplectic Euler: balls, blocks, springs, ropes, "
            "pendulums, ramps, pulleys, wheels, thrusters, friction &amp; drag</li>"
            "<li><b>Optics</b> — Ray tracing: lenses, mirrors (flat &amp; curved), "
            "prisms, filters, slits, diffraction gratings, beam splitters, polarisers</li>"
            "<li><b>Waves</b> — 2-D superposition: point sources, plane waves, "
            "barriers/slits, reflective walls, absorbers, ripple pulses</li>"
            "</ul>"
            "<p>Built with <a href='https://www.qt.io'>Qt 6</a> "
            "and <a href='https://eigen.tuxfamily.org'>Eigen 3</a>.</p>"
            "<p><b>Shortcuts:</b> Space Play/Pause · F5 Play · F6 Pause · "
            "R Rotate selection · Ctrl+0 Fit · Del Delete</p>");
    });
    helpMenu->addAction("Keyboard Shortcuts", this, [this] {
        QMessageBox::information(this, "Keyboard Shortcuts",
            "Space              Play / Pause\n"
            "F5                 Play\n"
            "F6                 Pause\n"
            "R                  Rotate selected component 15°\n"
            "\n"
            "Ctrl+N             New model\n"
            "Ctrl+O             Open model\n"
            "Ctrl+S             Save model\n"
            "Ctrl+Shift+S       Save As…\n"
            "Ctrl+0             Fit in view\n"
            "Ctrl++             Zoom in\n"
            "Ctrl+-             Zoom out\n"
            "Ctrl+Scroll        Zoom in / out\n"
            "\n"
            "Ctrl+Z             Undo\n"
            "Ctrl+Y             Redo\n"
            "Delete             Delete selected");
    });
}

void MainWindow::buildToolbar()
{
    auto* toolbar = addToolBar("Main");
    toolbar->setObjectName("mainToolbar");

    // ── File controls ────────────────────────────────────────────────────────
    toolbar->addAction("New",  this, &MainWindow::newModel);
    toolbar->addAction("Open", this, &MainWindow::openModel);
    toolbar->addAction("Save", this, &MainWindow::saveModel);
    toolbar->addSeparator();

    // ── Undo / Redo ──────────────────────────────────────────────────────────
    toolbar->addAction(undoRedoStack->qtStack()->createUndoAction(this, "Undo"));
    toolbar->addAction(undoRedoStack->qtStack()->createRedoAction(this, "Redo"));
    toolbar->addSeparator();

    // ── Simulation controls ──────────────────────────────────────────────────
    toolbar->addAction("▶ Play",  simulationLoop, &SimulationLoop::start);
    toolbar->addAction("⏸ Pause", simulationLoop, &SimulationLoop::pause);
    toolbar->addAction("↺ Reset", this, [this] {
        QMetaObject::invokeMethod(simulationLoop, [this]() { simulationLoop->reset(); });
        dataLogger->clearData();
    });
    toolbar->addSeparator();

    // ── Simulation speed ─────────────────────────────────────────────────────
    toolbar->addWidget(new QLabel("Speed: "));
    auto* speedCombo = new QComboBox();
    speedCombo->addItem("0.25×", 0.25);
    speedCombo->addItem("0.5×",  0.50);
    speedCombo->addItem("1×",    1.00);
    speedCombo->addItem("2×",    2.00);
    speedCombo->addItem("4×",    4.00);
    speedCombo->setCurrentIndex(2);   // default: 1×
    speedCombo->setToolTip("Simulation speed multiplier");
    connect(speedCombo, &QComboBox::currentIndexChanged,
            this, [this, speedCombo](int) {
                const double spd = speedCombo->currentData().toDouble();
                QMetaObject::invokeMethod(simulationLoop, [this, spd]() {
                    simulationLoop->setSpeed(spd);
                });
            });
    toolbar->addWidget(speedCombo);
    toolbar->addSeparator();

    // ── View controls ────────────────────────────────────────────────────────
    toolbar->addAction("Fit", this, &MainWindow::fitView)->setToolTip("Fit in view (Ctrl+0)");
}

void MainWindow::buildDocks()
{
    // ── Contents panel (Crocodile Physics-style curriculum browser) ───────────
    // Placed on the LEFT side above the Parts panel.  Double-clicking a
    // practical opens it as if the user had used File → Open.
    contentsPanel = new ContentsPanel(this);
    auto* contentsDock = new QDockWidget("Contents", this);
    contentsDock->setObjectName("contentsDock");
    contentsDock->setWidget(contentsPanel);
    contentsDock->setMinimumWidth(220);
    addDockWidget(Qt::LeftDockWidgetArea, contentsDock);

    // Built-in practicals are bundled as Qt resources — reuse openExample()
    // which handles resource paths (:/practicals/...) correctly.
    connect(contentsPanel, &ContentsPanel::builtinPracticalRequested,
            this, &MainWindow::openExample);

    // User-saved practicals live on disk — load via the normal path.
    connect(contentsPanel, &ContentsPanel::userPracticalRequested,
            this, [this](const QString& filePath) {
                if (loadModelFrom(filePath)) {
                    currentProjectPath = filePath;
                    m_dirty = false;
                    updateWindowTitle();
                    statusBar()->showMessage(QString("Opened %1").arg(filePath), 3000);
                }
            });

    // ── Parts panel ───────────────────────────────────────────────────────────
    auto* partsDock = new QDockWidget("Parts", this);
    partsDock->setObjectName("partsDock");
    partsDock->setWidget(new PartsPanel(partsDock));
    addDockWidget(Qt::LeftDockWidgetArea, partsDock);

    // Stack the two left docks: Contents on top, Parts below.
    tabifyDockWidget(contentsDock, partsDock);
    contentsDock->raise();   // show Contents tab first

    auto* propertiesDock = new QDockWidget("Properties", this);
    propertiesDock->setObjectName("propertiesDock");
    propertiesDock->setWidget(propertiesPanel);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    // Graph panel — docked at the bottom; shows live time-series data.
    graphPanel = new GraphPanel(dataLogger, this);
    auto* graphDock = new QDockWidget("Live Graph", this);
    graphDock->setObjectName("graphDock");
    graphDock->setWidget(graphPanel);
    graphDock->setMinimumHeight(140);
    addDockWidget(Qt::BottomDockWidgetArea, graphDock);
}

void MainWindow::connectCanvasSelection()
{
    connect(canvasView->graphicsScene(), &QGraphicsScene::selectionChanged, this, [this]() {
        const QList<QGraphicsItem*> selectedItems = canvasView->graphicsScene()->selectedItems();
        if (selectedItems.isEmpty()) {
            propertiesPanel->setComponent(nullptr);
            return;
        }

        auto* component = dynamic_cast<BaseComponent*>(selectedItems.first());
        propertiesPanel->setComponent(component);
    });
}

void MainWindow::updateWindowTitle()
{
    const QString appName  = "Physics Studio";
    const QString fileName = currentProjectPath.isEmpty()
        ? QString("untitled")
        : QFileInfo(currentProjectPath).fileName();
    const QString dirty    = m_dirty ? QString(" *") : QString();
    setWindowTitle(appName + " — " + fileName + dirty);
}

void MainWindow::newModel()
{
    if (m_dirty) {
        const auto btn = QMessageBox::question(
            this,
            "Unsaved Changes",
            "The current model has unsaved changes.\n"
            "Do you want to save before creating a new model?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if (btn == QMessageBox::Save) {
            saveModel();
            if (m_dirty) return;   // save was cancelled
        } else if (btn == QMessageBox::Cancel) {
            return;
        }
    }

    // Ensure the simulation has fully stopped before touching the scene.
    // BlockingQueuedConnection blocks the main thread until the worker
    // processes the pause — at most one tick (~16 ms) of wait time.
    QMetaObject::invokeMethod(simulationLoop, [this]() { simulationLoop->pause(); },
                              Qt::BlockingQueuedConnection);
    canvasView->clearComponents();
    undoRedoStack->qtStack()->clear();
    dataLogger->clearData();
    currentProjectPath.clear();
    propertiesPanel->setComponent(nullptr);
    m_dirty = false;
    updateWindowTitle();
    statusBar()->showMessage("New model", 3000);
}

void MainWindow::openModel()
{
    if (m_dirty) {
        const auto btn = QMessageBox::question(
            this,
            "Unsaved Changes",
            "The current model has unsaved changes.\n"
            "Do you want to save before opening another model?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if (btn == QMessageBox::Save) {
            saveModel();
            if (m_dirty) return;
        } else if (btn == QMessageBox::Cancel) {
            return;
        }
    }

    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open Physics Simulation Studio Model",
        QString(),
        "Physics Simulation Studio (*.pss);;JSON (*.json)");
    if (path.isEmpty()) {
        return;
    }

    if (loadModelFrom(path)) {
        currentProjectPath = path;
        m_dirty = false;
        updateWindowTitle();
        statusBar()->showMessage(QString("Opened %1").arg(path), 3000);
    }
}

void MainWindow::saveModel()
{
    QString path = currentProjectPath;
    if (path.isEmpty()) {
        // Default to My Content folder so the model appears in the Contents panel.
        const QString defaultDir =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + "/PhysicsStudio/My Content/";
        path = QFileDialog::getSaveFileName(
            this,
            "Save Physics Simulation Studio Model",
            defaultDir,
            "Physics Simulation Studio (*.pss);;JSON (*.json)");
    }

    if (path.isEmpty()) {
        return;
    }

    if (saveModelAs(path)) {
        currentProjectPath = path;
        m_dirty = false;
        updateWindowTitle();
        statusBar()->showMessage(QString("Saved %1").arg(path), 3000);
        // If saved into the My Content folder the watcher picks it up, but
        // call refreshMyContent() explicitly for saves elsewhere too.
        if (contentsPanel) contentsPanel->refreshMyContent();
    }
}

void MainWindow::saveModelAsDialog()
{
    // Default to the My Content folder so saves appear in the Contents panel.
    const QString defaultPath = currentProjectPath.isEmpty()
        ? (QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
           + "/PhysicsStudio/My Content/")
        : currentProjectPath;

    const QString path = QFileDialog::getSaveFileName(
        this,
        "Save Physics Simulation Studio Model As",
        defaultPath,
        "Physics Simulation Studio (*.pss);;JSON (*.json)");

    if (path.isEmpty())
        return;

    if (saveModelAs(path)) {
        currentProjectPath = path;
        m_dirty = false;
        updateWindowTitle();
        statusBar()->showMessage(QString("Saved %1").arg(path), 3000);
        if (contentsPanel) contentsPanel->refreshMyContent();
    }
}

bool MainWindow::saveModelAs(const QString& path)
{
    ProjectDocument document;
    document.components = canvasView->componentsToJson();
    document.wires = canvasView->wiresToJson();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Save Failed", QString("Could not write %1").arg(path));
        return false;
    }

    file.write(QJsonDocument(document.toJson()).toJson(QJsonDocument::Indented));
    return true;
}

bool MainWindow::loadModelFrom(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Open Failed", QString("Could not read %1").arg(path));
        return false;
    }

    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject()) {
        QMessageBox::warning(this, "Open Failed", "The selected file is not a valid model file.");
        return false;
    }

    // Pause before modifying the scene to avoid a race with the worker thread.
    QMetaObject::invokeMethod(simulationLoop, [this]() { simulationLoop->pause(); },
                              Qt::BlockingQueuedConnection);

    const ProjectDocument document = ProjectDocument::fromJson(json.object());
    canvasView->loadScene(document.components, document.wires);
    undoRedoStack->qtStack()->clear();
    propertiesPanel->setComponent(nullptr);
    refreshSimulationDomain();
    m_dirty = false;      // loading is not a user edit — clear the flag set by loadScene
    return true;
}

void MainWindow::refreshSimulationDomain()
{
    refreshElectronicsDomain();
    refreshMotionDomain();
    refreshOpticsDomain();
    refreshWaveDomain();

    // Rebuild the data logger channel list from the updated scene.
    dataLogger->setComponents(canvasView->components());
}

void MainWindow::refreshElectronicsDomain()
{
    ElectronicsDomain domain;
    for (auto* comp : canvasView->components()) {
        for (auto* pad : comp->pads) {
            if (pad->domain == DomainType::Electrical) {
                domain.components.append(comp);
                break;
            }
        }
    }
    domain.wires = canvasView->wires();
    // Post to worker thread — domain is moved into the lambda's closure so it
    // remains valid until the worker processes the queued call.
    QMetaObject::invokeMethod(simulationLoop,
        [this, d = std::move(domain)]() mutable {
            simulationLoop->setElectronicsDomain(std::move(d));
        }, Qt::QueuedConnection);
}

void MainWindow::refreshMotionDomain()
{
    MotionDomain domain;

    // Use the scene rect as the physics boundary.
    const QRectF sr = canvasView->graphicsScene()->sceneRect();
    domain.boundaryLeft   = sr.left();
    domain.boundaryRight  = sr.right();
    domain.boundaryTop    = sr.top();
    domain.boundaryBottom = sr.bottom();

    // Map from component pointer → index in domain.bodies (for spring/rope wiring).
    QMap<BaseComponent*, int> bodyIndex;

    // --- Pass 1: build bodies from Ball / Block / Anchor components. ---
    for (auto* comp : canvasView->components()) {
        bool isMechanical = false;
        for (auto* pad : comp->pads) {
            if (pad->domain == DomainType::Mechanical) { isMechanical = true; break; }
        }
        if (!isMechanical) continue;

        if (comp->typeId == "MOT_BALL") {
            MotionBody body;
            body.component  = comp;
            body.shape      = MotionBody::Shape::Ball;
            body.mass       = comp->properties.value("mass",        1.0).toDouble();
            body.radius     = comp->properties.value("radius",     20.0).toDouble();
            body.restitution= comp->properties.value("restitution",  0.8).toDouble();
            body.friction   = comp->properties.value("friction",    0.0).toDouble();
            body.airDrag    = comp->properties.value("airDrag",     0.0).toDouble();
            body.fixed      = false;
            body.pos        = comp->pos();
            body.vel        = QPointF(comp->properties.value("velocityX", 0.0).toDouble(),
                                      comp->properties.value("velocityY", 0.0).toDouble());
            bodyIndex[comp] = domain.bodies.size();
            domain.bodies.append(body);

            comp->simState.remove("vx");
            comp->simState.remove("vy");
            comp->simState.remove("speed");
            comp->update();

        } else if (comp->typeId == "MOT_WHEEL") {
            MotionBody body;
            body.component  = comp;
            body.shape      = MotionBody::Shape::Ball;   // circle shape for collision
            body.isWheel    = true;
            body.mass       = comp->properties.value("mass",        2.0).toDouble();
            body.radius     = comp->properties.value("radius",     24.0).toDouble();
            body.restitution= comp->properties.value("restitution",  0.5).toDouble();
            body.friction   = comp->properties.value("friction",    0.15).toDouble();
            body.fixed      = false;
            body.pos        = comp->pos();
            body.vel        = QPointF(comp->properties.value("velocityX", 0.0).toDouble(),
                                      comp->properties.value("velocityY", 0.0).toDouble());
            // Restore rolling angle from simState if simulation was paused.
            body.angle      = comp->simState.value("angle", 0.0).toDouble();
            body.angularVel = comp->simState.value("angularVel", 0.0).toDouble();
            bodyIndex[comp] = domain.bodies.size();
            domain.bodies.append(body);

            comp->simState.remove("vx");
            comp->simState.remove("vy");
            comp->simState.remove("speed");
            comp->update();

        } else if (comp->typeId == "MOT_BLOCK") {
            MotionBody body;
            body.component  = comp;
            body.shape      = MotionBody::Shape::Block;
            body.mass       = comp->properties.value("mass",        5.0).toDouble();
            body.halfW      = comp->properties.value("halfW",      40.0).toDouble();
            body.halfH      = comp->properties.value("halfH",      20.0).toDouble();
            body.restitution= comp->properties.value("restitution",  0.5).toDouble();
            body.friction   = comp->properties.value("friction",    0.0).toDouble();
            body.fixed      = comp->properties.value("fixed", false).toBool() || body.mass <= 0.0;
            body.pos        = comp->pos();
            body.vel        = QPointF(0.0, 0.0);
            bodyIndex[comp] = domain.bodies.size();
            domain.bodies.append(body);

        } else if (comp->typeId == "MOT_ANCHOR" || comp->typeId == "MOT_PULLEY") {
            // Both anchors and pulleys act as fixed immovable points.
            MotionBody body;
            body.component  = comp;
            body.shape      = MotionBody::Shape::Ball;
            body.mass       = 1.0;
            body.radius     = (comp->typeId == "MOT_PULLEY")
                                  ? comp->properties.value("radius", 24.0).toDouble()
                                  : 6.0;
            body.restitution= 1.0;
            body.fixed      = true;
            body.pos        = comp->pos();
            body.vel        = QPointF(0.0, 0.0);
            bodyIndex[comp] = domain.bodies.size();
            domain.bodies.append(body);
        }
    }

    // --- Pass 2: build springs and ropes. ---
    for (auto* comp : canvasView->components()) {
        if (comp->typeId != "MOT_SPRING" && comp->typeId != "MOT_ROPE") continue;

        auto resolveBody = [&](const QString& padId, QPointF& outAnchor) -> int {
            const ConnectionPad* pad = comp->padById(padId);
            if (!pad || pad->connectedWires.isEmpty()) {
                outAnchor = comp->mapToScene(pad ? pad->localPos : QPointF());
                return -1;
            }
            const Wire* wire = pad->connectedWires.first();
            BaseComponent* other = (wire->startPad == pad)
                ? wire->endComponent : wire->startComponent;
            if (other && bodyIndex.contains(other))
                return bodyIndex[other];
            outAnchor = comp->mapToScene(pad->localPos);
            return -1;
        };

        MotionSpring spring;
        spring.bodyA     = resolveBody("a", spring.anchorA);
        spring.bodyB     = resolveBody("b", spring.anchorB);
        if (comp->typeId == "MOT_ROPE") {
            spring.stiffness  = comp->properties.value("stiffness",  2000.0).toDouble();
            spring.restLength = comp->properties.value("length",      120.0).toDouble();
            spring.damping    = comp->properties.value("damping",       5.0).toDouble();
        } else {
            spring.stiffness  = comp->properties.value("stiffness",  200.0).toDouble();
            spring.restLength = comp->properties.value("restLength", 100.0).toDouble();
            spring.damping    = comp->properties.value("damping",      2.0).toDouble();
        }
        spring.springComponent = comp;
        domain.springs.append(spring);

        comp->simState.remove("hasLiveEndpoints");
        comp->simState.remove("taut");
        comp->update();
    }

    // --- Pass 3: build pendulums. ---
    for (auto* comp : canvasView->components()) {
        if (comp->typeId != "MOT_PENDULUM") continue;

        MotionPendulum pend;
        pend.component = comp;
        pend.pivot     = comp->pos();
        pend.length    = comp->properties.value("length",    120.0).toDouble();
        pend.damping   = comp->properties.value("damping",     0.05).toDouble();
        pend.bobRadius = comp->properties.value("bobRadius",  12.0).toDouble();

        // Restore angle/omega from simState if present (persists across pauses).
        pend.angle = comp->simState.contains("angle")
            ? comp->simState["angle"].toDouble()
            : comp->properties.value("angle", 30.0).toDouble() * M_PI / 180.0;
        pend.omega = comp->simState.value("omega", 0.0).toDouble();

        domain.pendulums.append(pend);
    }

    // --- Pass 4: build ramps. ---
    for (auto* comp : canvasView->components()) {
        if (comp->typeId != "MOT_RAMP") continue;

        const double L = comp->properties.value("length", 140.0).toDouble();
        MotionRamp ramp;
        ramp.component   = comp;
        ramp.p1          = comp->mapToScene(QPointF(-L * 0.5, 0.0));
        ramp.p2          = comp->mapToScene(QPointF( L * 0.5, 0.0));
        ramp.restitution = comp->properties.value("restitution", 0.5).toDouble();
        domain.ramps.append(ramp);
    }

    // --- Pass 5: build thrusters. ---
    for (auto* comp : canvasView->components()) {
        if (comp->typeId != "MOT_THRUSTER") continue;

        const double forceMag = comp->properties.value("force", 500.0).toDouble();
        const double angleDeg = comp->properties.value("angle",   0.0).toDouble();
        const double angleRad = angleDeg * M_PI / 180.0;

        // angle=0 → upward (−Y); convert to world-space components.
        MotionThruster thr;
        thr.component = comp;
        thr.forceX    =  forceMag * std::sin(angleRad);
        thr.forceY    = -forceMag * std::cos(angleRad);

        // Resolve the connected body via the "attach" pad.
        const ConnectionPad* pad = comp->padById("attach");
        thr.bodyIdx = -1;
        if (pad && !pad->connectedWires.isEmpty()) {
            const Wire* wire = pad->connectedWires.first();
            BaseComponent* other = (wire->startPad == pad)
                ? wire->endComponent : wire->startComponent;
            if (other && bodyIndex.contains(other))
                thr.bodyIdx = bodyIndex[other];
        }

        domain.thrusters.append(thr);
    }

    QMetaObject::invokeMethod(simulationLoop,
        [this, d = std::move(domain)]() mutable {
            simulationLoop->setMotionDomain(std::move(d));
        }, Qt::QueuedConnection);
}

void MainWindow::refreshOpticsDomain()
{
    OpticalDomain domain;
    for (auto* comp : canvasView->components()) {
        const QString& t = comp->typeId;
        if (t == "OPT_SRC"      || t == "OPT_MIRROR"  || t == "OPT_LENS"
         || t == "OPT_SCREEN"  || t == "OPT_PRISM"   || t == "OPT_FILTER"
         || t == "OPT_SLIT"    || t == "OPT_CONCAVE"  || t == "OPT_GRATING"
         || t == "OPT_SPLITTER"|| t == "OPT_POLARISER")
            domain.components << comp;
    }
    // Post domain setter + static trace as a single queued call so they
    // execute atomically in order on the worker thread.  traceOpticsOnce()
    // emits opticsUpdated → overlay updates on the main thread automatically.
    QMetaObject::invokeMethod(simulationLoop,
        [this, d = std::move(domain)]() mutable {
            simulationLoop->setOpticalDomain(std::move(d));
            simulationLoop->traceOpticsOnce();
        }, Qt::QueuedConnection);
}

void MainWindow::refreshWaveDomain()
{
    WaveDomain domain;

    for (auto* comp : canvasView->components()) {
        const QString& t = comp->typeId;
        if (t == "WAV_SRC" || t == "WAV_SOUND" || t == "WAV_RIPPLE")
            domain.sources << comp;
        else if (t == "WAV_DET")
            domain.detectors << comp;
        else if (t == "WAV_BARRIER")
            domain.barriers << comp;
        else if (t == "WAV_WALL")
            domain.walls << comp;
        else if (t == "WAV_ABSORBER")
            domain.absorbers << comp;
        else if (t == "WAV_PLANE")
            domain.planeSources << comp;
    }

    // Clear detector readings on the main thread (sim is paused — safe).
    for (auto* comp : domain.detectors) {
        comp->simState.remove("amplitude");
        comp->update();
    }

    // Post domain setter to worker thread (domain is moved into closure).
    QMetaObject::invokeMethod(simulationLoop,
        [this, d = std::move(domain)]() mutable {
            simulationLoop->setWaveDomain(std::move(d));
        }, Qt::QueuedConnection);
    // Wave field overlay will be cleared / updated on the next tick once the
    // simulation resumes and emits waveFieldUpdated.
}

void MainWindow::openExample(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Example Error",
            QString("Could not load example:\n%1").arg(resourcePath));
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isObject()) {
        QMessageBox::warning(this, "Example Error", "Invalid example file format.");
        return;
    }

    QMetaObject::invokeMethod(simulationLoop, [this]() { simulationLoop->pause(); },
                              Qt::BlockingQueuedConnection);
    dataLogger->clearData();

    const ProjectDocument doc = ProjectDocument::fromJson(json.object());
    canvasView->loadScene(doc.components, doc.wires);
    undoRedoStack->qtStack()->clear();
    propertiesPanel->setComponent(nullptr);
    currentProjectPath.clear();

    refreshSimulationDomain();
    m_dirty = false;      // opening an example is not a "dirty" change
    updateWindowTitle();
    statusBar()->showMessage("Example loaded — press Play to start", 4000);
}
