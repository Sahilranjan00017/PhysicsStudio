#include "app/MainWindow.h"

#include "canvas/CanvasView.h"
#include "components/BaseComponent.h"
#include "components/ComponentRegistry.h"
#include "interaction/UndoRedoStack.h"
#include "simulation/SimulationLoop.h"
#include "ui/partspanel/PartsPanel.h"
#include "ui/properties/PropertiesPanel.h"

#include <QAction>
#include <QDockWidget>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QList>
#include <QMenuBar>
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
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenus()
{
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("New Model");
    fileMenu->addAction("Open Model");
    fileMenu->addAction("Save");
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(undoRedoStack->qtStack()->createUndoAction(this, "Undo"));
    editMenu->addAction(undoRedoStack->qtStack()->createRedoAction(this, "Redo"));

    auto* simulationMenu = menuBar()->addMenu("&Simulation");
    simulationMenu->addAction("Play", simulationLoop, &SimulationLoop::start);
    simulationMenu->addAction("Pause", simulationLoop, &SimulationLoop::pause);
    simulationMenu->addAction("Reset", simulationLoop, &SimulationLoop::reset);
}

void MainWindow::buildToolbar()
{
    auto* toolbar = addToolBar("Main");
    toolbar->addAction("New");
    toolbar->addAction("Open");
    toolbar->addAction("Save");
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
