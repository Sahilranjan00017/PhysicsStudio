#include "app/MainWindow.h"

#include "canvas/CanvasView.h"
#include "simulation/SimulationLoop.h"

#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      canvasView(new CanvasView(this)),
      simulationLoop(new SimulationLoop(this))
{
    setWindowTitle("Physics Simulation Studio");
    resize(1280, 800);
    setCentralWidget(canvasView);

    buildMenus();
    buildToolbar();
    buildDocks();
    statusBar()->showMessage("Ready");
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
    toolbar->addAction("Play", simulationLoop, &SimulationLoop::start);
    toolbar->addAction("Pause", simulationLoop, &SimulationLoop::pause);
    toolbar->addAction("Reset", simulationLoop, &SimulationLoop::reset);
}

void MainWindow::buildDocks()
{
    auto* partsDock = new QDockWidget("Parts", this);
    partsDock->setWidget(new QLabel("Parts library placeholder", partsDock));
    addDockWidget(Qt::LeftDockWidgetArea, partsDock);

    auto* propertiesDock = new QDockWidget("Properties", this);
    propertiesDock->setWidget(new QLabel("Properties placeholder", propertiesDock));
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);
}

