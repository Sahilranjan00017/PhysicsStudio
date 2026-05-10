#include "app/MainWindow.h"

#include "canvas/CanvasView.h"
#include "components/BaseComponent.h"
#include "components/ComponentRegistry.h"
#include "components/ConnectionPad.h"
#include "components/Wire.h"
#include "interaction/UndoRedoStack.h"
#include "persistence/ProjectDocument.h"
#include "simulation/SimulationLoop.h"
#include "simulation/electronics/ElectronicsSolver.h"
#include "simulation/motion/MotionSolver.h"
#include "ui/partspanel/PartsPanel.h"
#include "ui/properties/PropertiesPanel.h"

#include <QAction>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QIODevice>
#include <QJsonDocument>
#include <QKeySequence>
#include <QList>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointF>
#include <QStatusBar>
#include <QString>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      canvasView(new CanvasView(this)),
      propertiesPanel(new PropertiesPanel(this)),
      simulationLoop(new SimulationLoop(this)),
      undoRedoStack(new UndoRedoStack(this))
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

    connect(canvasView, &CanvasView::componentPlaced, this, [this](const QString& typeId, const QPointF& position) {
        statusBar()->showMessage(
            QString("Placed %1 at (%2, %3)")
                .arg(typeId)
                .arg(position.x(), 0, 'f', 0)
                .arg(position.y(), 0, 'f', 0),
            3000);
        refreshSimulationDomain();
    });

    // Rebuild domains whenever the scene changes structurally.
    // Guard: skip while the simulation is running — the motion solver moves items
    // each tick, which would trigger changed() and reset physics state.
    connect(canvasView->graphicsScene(), &QGraphicsScene::changed,
            this, [this](const QList<QRectF>&) {
                if (!simulationLoop->isRunning())
                    refreshSimulationDomain();
            });

    // After each solver tick, repaint the canvas so live values show up.
    connect(simulationLoop, &SimulationLoop::tickComplete,
            this, [this](double) { canvasView->viewport()->update(); });
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus()
{
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Model", this, &MainWindow::newModel);
    fileMenu->addAction("Open Model", this, &MainWindow::openModel);
    fileMenu->addAction("Save", this, &MainWindow::saveModel);
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoRedoStack->qtStack()->createUndoAction(this, "Undo"));
    editMenu->addAction(undoRedoStack->qtStack()->createRedoAction(this, "Redo"));
    auto* deleteAction = editMenu->addAction("Delete", canvasView, &CanvasView::deleteSelectedComponents);
    deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));

    auto* simulationMenu = menuBar()->addMenu("&Simulation");
    simulationMenu->addAction("Play", simulationLoop, &SimulationLoop::start);
    simulationMenu->addAction("Pause", simulationLoop, &SimulationLoop::pause);
    simulationMenu->addAction("Reset", simulationLoop, &SimulationLoop::reset);
}

void MainWindow::buildToolbar()
{
    auto* toolbar = addToolBar("Main");
    toolbar->addAction("New", this, &MainWindow::newModel);
    toolbar->addAction("Open", this, &MainWindow::openModel);
    toolbar->addAction("Save", this, &MainWindow::saveModel);
    toolbar->addSeparator();
    toolbar->addAction(undoRedoStack->qtStack()->createUndoAction(this, "Undo"));
    toolbar->addAction(undoRedoStack->qtStack()->createRedoAction(this, "Redo"));
    toolbar->addSeparator();
    toolbar->addAction("Play", simulationLoop, &SimulationLoop::start);
    toolbar->addAction("Pause", simulationLoop, &SimulationLoop::pause);
    toolbar->addAction("Reset", simulationLoop, &SimulationLoop::reset);
}

void MainWindow::buildDocks()
{
    auto* partsDock = new QDockWidget("Parts", this);
    partsDock->setWidget(new PartsPanel(partsDock));
    addDockWidget(Qt::LeftDockWidgetArea, partsDock);

    auto* propertiesDock = new QDockWidget("Properties", this);
    propertiesDock->setWidget(propertiesPanel);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);
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

void MainWindow::newModel()
{
    canvasView->clearComponents();
    undoRedoStack->qtStack()->clear();
    currentProjectPath.clear();
    propertiesPanel->setComponent(nullptr);
    statusBar()->showMessage("New model", 3000);
}

void MainWindow::openModel()
{
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
        statusBar()->showMessage(QString("Opened %1").arg(path), 3000);
    }
}

void MainWindow::saveModel()
{
    QString path = currentProjectPath;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this,
            "Save Physics Simulation Studio Model",
            QString(),
            "Physics Simulation Studio (*.pss);;JSON (*.json)");
    }

    if (path.isEmpty()) {
        return;
    }

    if (saveModelAs(path)) {
        currentProjectPath = path;
        statusBar()->showMessage(QString("Saved %1").arg(path), 3000);
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

    const ProjectDocument document = ProjectDocument::fromJson(json.object());
    canvasView->loadScene(document.components, document.wires);
    undoRedoStack->qtStack()->clear();
    propertiesPanel->setComponent(nullptr);
    refreshSimulationDomain();
    return true;
}

void MainWindow::refreshSimulationDomain()
{
    refreshElectronicsDomain();
    refreshMotionDomain();
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
    simulationLoop->setElectronicsDomain(std::move(domain));
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

    // Map from component pointer → index in domain.bodies (for spring wiring).
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
            body.fixed      = false;
            body.pos        = comp->pos();
            body.vel        = QPointF(comp->properties.value("velocityX", 0.0).toDouble(),
                                      comp->properties.value("velocityY", 0.0).toDouble());
            bodyIndex[comp] = domain.bodies.size();
            domain.bodies.append(body);

            // Clear stale sim-state so the ball redraws cleanly.
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
            body.fixed      = comp->properties.value("fixed", false).toBool() || body.mass <= 0.0;
            body.pos        = comp->pos();
            body.vel        = QPointF(0.0, 0.0);
            bodyIndex[comp] = domain.bodies.size();
            domain.bodies.append(body);

        } else if (comp->typeId == "MOT_ANCHOR") {
            MotionBody body;
            body.component  = comp;
            body.shape      = MotionBody::Shape::Ball;
            body.mass       = 1.0;
            body.radius     = 6.0;
            body.restitution= 1.0;
            body.fixed      = true;
            body.pos        = comp->pos();
            body.vel        = QPointF(0.0, 0.0);
            bodyIndex[comp] = domain.bodies.size();
            domain.bodies.append(body);
        }
    }

    // --- Pass 2: build springs from Spring components. ---
    for (auto* comp : canvasView->components()) {
        if (comp->typeId != "MOT_SPRING") continue;

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
        spring.bodyA       = resolveBody("a", spring.anchorA);
        spring.bodyB       = resolveBody("b", spring.anchorB);
        spring.stiffness   = comp->properties.value("stiffness",  200.0).toDouble();
        spring.restLength  = comp->properties.value("restLength", 100.0).toDouble();
        spring.damping     = comp->properties.value("damping",      2.0).toDouble();
        spring.springComponent = comp;
        domain.springs.append(spring);

        // Clear stale live-endpoint flag so the spring redraws statically.
        comp->simState.remove("hasLiveEndpoints");
        comp->update();
    }

    simulationLoop->setMotionDomain(std::move(domain));
}
