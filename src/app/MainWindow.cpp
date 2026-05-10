#include "app/MainWindow.h"

#include "canvas/CanvasView.h"
#include "components/BaseComponent.h"
#include "components/ComponentRegistry.h"
#include "interaction/UndoRedoStack.h"
#include "persistence/ProjectDocument.h"
#include "simulation/SimulationLoop.h"
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
    });
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
    canvasView->loadComponents(document.components);
    undoRedoStack->qtStack()->clear();
    propertiesPanel->setComponent(nullptr);
    return true;
}
