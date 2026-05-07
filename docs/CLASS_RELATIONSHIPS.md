# Class Relationships & System Structure

This document visualizes the class hierarchy, composition relationships, and signal/slot connections in Physics Simulation Studio.

---

## Inheritance Hierarchies

### Qt Framework Layer

```
QObject (Qt root)
├── QWidget
│   ├── QMainWindow
│   │   └── MainWindow
│   └── QGraphicsView
│       └── CanvasView
├── QGraphicsObject
│   └── BaseComponent (physics abstraction)
├── QGraphicsItem
│   ├── Wire (connection)
│   └── [rendered items]
└── QTimer
    └── [used by SimulationLoop]
```

### Physics Component Hierarchy

```
BaseComponent (QGraphicsObject)
│
├── Electronics Domain Components
│   ├── Resistor
│   ├── Capacitor
│   ├── Inductor
│   ├── VoltageSource
│   ├── CurrentSource
│   ├── Ammeter
│   └── Voltmeter
│
├── Motion Domain Components
│   ├── Ball
│   ├── Block
│   ├── Spring
│   └── Hinge
│
├── Optics Domain Components
│   ├── Mirror
│   ├── Lens
│   ├── Prism
│   └── Beamsplitter
│
└── Wave Domain Components
    ├── WaveSource
    ├── WaveDetector
    ├── WaveBoundary
    └── WaveMaterial
```

### Solver Hierarchy

```
IElectronicsSolver (interface)
└── ElectronicsSolver

IMotionSolver (interface)
└── MotionSolver

IOpticsSolver (interface)
└── OpticsSolver

IWaveSolver (interface)
└── WaveSolver
```

---

## Composition/Aggregation Structure

### MainWindow Owns/Manages

```
MainWindow (QMainWindow)
│
├── CanvasView* (central widget)
│   └── QGraphicsScene*
│       ├── BaseComponent instances (via addItem)
│       └── Wire instances (via addItem)
│
├── SimulationLoop* (QObject)
│   ├── QTimer (for 60 FPS)
│   ├── ElectronicsSolver*
│   ├── MotionSolver*
│   ├── OpticsSolver*
│   └── WaveSolver*
│
├── UndoRedoStack* (QObject)
│   └── QUndoStack (internal)
│
├── ProjectDocument* (persistent state)
│   └── Scene[]
│       ├── Component[] (serialized)
│       └── Wire[] (serialized)
│
└── UI Elements (docks, menus, toolbars)
    ├── Parts Panel (QDockWidget)
    │   └── Component library
    ├── Properties Panel (QDockWidget)
    │   └── Property editors
    └── [Menus, Toolbars]
```

### CanvasView Owns/Manages

```
CanvasView (QGraphicsView)
│
└── QGraphicsScene*
    ├── BaseComponent* (list)
    │   ├── Resistor1 (id="uuid1")
    │   ├── Ball1 (id="uuid2")
    │   ├── Mirror1 (id="uuid3")
    │   └── [more components]
    │
    └── Wire* (list)
        ├── Wire1 (startPad → endPad)
        ├── Wire2 (startPad → endPad)
        └── [more wires]
```

### BaseComponent Structure

```
BaseComponent
│
├── Data Members
│   ├── id (QString)
│   ├── typeId (QString)
│   ├── displayName (QString)
│   ├── properties (QMap<QString, QVariant>)
│   ├── simState (QMap<QString, QVariant>)
│   ├── pads (QList<ConnectionPad*>) — non-owning
│   └── [rotation, lock flags, etc]
│
├── Virtual Methods (overridden by subclasses)
│   ├── stampMNA(double dt)
│   ├── stepMotion(MotionContext&, double dt)
│   ├── traceRays(RayContext&)
│   └── stepWave(WaveContext&, double dt)
│
├── Serialization
│   ├── toJson() → QJsonObject
│   └── fromJson(QJsonObject)
│
└── Qt Signals
    ├── propertyChanged(QString key, QVariant value)
    ├── destroyedSignal(BaseComponent* comp)
    └── repairedSignal(BaseComponent* comp)
```

### ConnectionPad Structure

```
ConnectionPad (struct, non-owning)
│
├── localPos (QPointF) — relative to component
├── type (PadType) — Input / Output / Bidirectional
├── domain (DomainType) — Electrical / Mechanical / Optical / Wave
├── padId (QString)
├── connectedWires (QList<Wire*>) — non-owning
└── highlighted (bool)
```

### Wire Structure

```
Wire (QGraphicsItem)
│
├── points (QList<QPointF>) — routing waypoints
├── startPad (ConnectionPad*) — non-owning
├── endPad (ConnectionPad*) — non-owning
├── signal (QVariant) — propagated value
└── wireType (WireType) — Power / Ground / Signal / Optical / Mechanical
```

---

## Signal/Slot Connection Map

### MainWindow → Components

```
MainWindow::buildMenus()
  File Menu → [New, Open, Save, Exit]
  Simulation Menu → Play/Pause/Reset
  
[Play button] → SimulationLoop::start()
[Pause button] → SimulationLoop::pause()
[Reset button] → SimulationLoop::reset()
```

### Canvas Interactions

```
CanvasView::dropEvent()
  (user drags component from Parts Panel)
  ↓
CreateComponentCommand created
  ↓
UndoRedoStack::push(command)
  ↓
CreateComponentCommand::redo()
  ↓
BaseComponent created and added to scene
  ↓
canvas.update() [repaint]
```

### Component State Changes

```
BaseComponent::property changed
  ↓
emit propertyChanged(key, value)
  ↓
PropertiesPanel::onPropertyChanged()
  ├─ Update UI display
  └─ (optional) Create EditPropertyCommand
      ↓
      UndoRedoStack::push(command)
```

### Simulation Loop

```
SimulationLoop::tick() [every 16ms]
  ├─ ElectronicsSolver::solve(domain, dt)
  ├─ MotionSolver::step(domain, dt)
  ├─ OpticsSolver::trace(domain)
  ├─ WaveSolver::step(domain, dt)
  └─ emit tickComplete(simulationTime)
      ↓
      CanvasView receives signal
      └─ canvas.update() [repaint with new state]
```

### Component Lifecycle

```
User drags from Parts Panel
  ↓
CanvasView::dropEvent()
  ↓
CreateComponentCommand::execute()
  ├─ ComponentFactory::create(typeId)
  ├─ component->setPos(dropPosition)
  ├─ scene->addItem(component) ← component now rendered
  ├─ connect(component, &BaseComponent::propertyChanged, ...)
  └─ emit componentAdded(component)

[User runs simulation]
  ↓
SimulationLoop::tick() [repeats at 60 FPS]
  ├─ component->stampMNA(dt) or stepMotion(dt) [etc]
  └─ component->simState updated with results

[User deletes component via Ctrl+Del or Delete button]
  ↓
DeleteComponentCommand::execute()
  ├─ scene->removeItem(component)
  ├─ emit component->destroyedSignal()
  └─ [wires referencing this component are also deleted]

[User presses Ctrl+Z (undo)]
  ↓
DeleteComponentCommand::undo()
  ├─ scene->addItem(component) ← component restored
  └─ component re-rendered
```

---

## Dependency Graph

The dependency direction flows **inward** toward stable interfaces:

```
                    External
                    (User I/O)
                        ↑
                        │
                        ↓
┌───────────────────────────────────┐
│ Application Layer                 │
│ ├─ MainWindow (orchestrates)      │
│ ├─ Menus, Toolbars, Docks        │
│ └─ Status Bar / Logging           │
└──────────────────┬────────────────┘
                   │
                   ↓
┌───────────────────────────────────┐
│ Interaction Layer                 │
│ ├─ CanvasView (UI rendering)      │
│ ├─ UndoRedoStack (commands)       │
│ └─ DragDrop, Selection            │
└──────────────────┬────────────────┘
                   │
                   ↓
┌───────────────────────────────────┐
│ Component System (Stable)         │
│ ├─ BaseComponent (virtual methods)│
│ ├─ ConnectionPad (links)          │
│ └─ Wire (connections)             │
└──────────────────┬────────────────┘
                   │
                   ↓
┌───────────────────────────────────┐
│ Simulation Layer                  │
│ ├─ SimulationLoop (orchestrates)  │
│ ├─ Solver interfaces              │
│ └─ Domain contexts                │
└──────────────────┬────────────────┘
                   │
                   ↓
┌───────────────────────────────────┐
│ Domain-Specific Math              │
│ ├─ ElectronicsSolver (Eigen MNA)  │
│ ├─ MotionSolver (RK4 integration) │
│ ├─ OpticsSolver (ray tracing)     │
│ └─ WaveSolver (FDTD grid)         │
└───────────────────────────────────┘
```

### Rules

1. **Application layer** depends on everything below it
2. **Interaction layer** knows about components and commands
3. **Component system** is independent (doesn't know about solvers yet)
4. **Simulation layer** knows component interfaces and domain contexts
5. **Math layer** has no dependencies on UI or application logic

This separation keeps changes localized:
- New solver algorithm? Update math layer only
- New UI feature? Update application layer only
- New component type? Add to component system and appropriate solver

---

## Key Non-Owning Pointers

These are intentional non-owning pointers where ownership is clear and documented:

| Pointer | Owner | Notes |
|---------|-------|-------|
| `ConnectionPad* in pads` | BaseComponent | Pads are part of component |
| `Wire* in pad->connectedWires` | QGraphicsScene (via addItem) | Wires are scene items |
| `Wire->startPad` | BaseComponent | Pad belongs to component |
| `Wire->endPad` | BaseComponent | Pad belongs to component |
| `BaseComponent* in scene` | QGraphicsScene | Scene owns items |

---

## Thread Safety

**Current Design**: Single-threaded

- All UI updates on main thread
- SimulationLoop runs on main thread (Qt timer-based)
- No background solvers yet

Future consideration for Phase 3+: If solver performance requires it, move solvers to worker threads and use thread-safe queues for solver results.

---

## Class Count Summary

| Category | Count |
|----------|-------|
| Qt Framework Classes | 5 (QMainWindow, QWidget, etc.) |
| Application Classes | 6 (MainWindow, CanvasView, UndoRedoStack, ProjectDocument, etc.) |
| Component Classes | 16+ (Resistor, Ball, Mirror, WaveSource, etc.) |
| Solver Classes | 5 (interfaces + 4 implementations) |
| Context/Data Classes | 8+ (DomainType, DomainContext, ConnectionPad, Wire, etc.) |
| **Total** | **40+** |

---

## Ready for Phase 1 Implementation

This structure is stable and ready for Phase 1 work:

1. Component implementations will add concrete subclasses
2. Command implementations will add CreateComponentCommand, MoveComponentCommand, etc.
3. UI panels will add Parts Panel, Properties Panel components
4. All fit within this established class hierarchy

No architectural refactoring needed.
