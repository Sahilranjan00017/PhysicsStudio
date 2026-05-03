# Class Relationships

This document describes the Phase 0.1a class structure and the expected next relationships for Phase 1.

## Inheritance

```text
QObject
  -> QMainWindow
       -> MainWindow

QWidget
  -> QFrame
       -> QAbstractScrollArea
            -> QGraphicsView
                 -> CanvasView

QObject
  -> QGraphicsItem
       -> QGraphicsObject
            -> BaseComponent
                 -> ResistorComponent        (planned)
                 -> VoltageSourceComponent   (planned)
                 -> BallComponent            (planned)
                 -> MirrorComponent          (planned)
                 -> WaveSourceComponent      (planned)

QGraphicsItem
  -> Wire

QObject
  -> SimulationLoop
  -> UndoRedoStack
```

## Composition

```text
MainWindow
  owns CanvasView
  owns SimulationLoop
  owns Parts dock widget
  owns Properties dock widget

CanvasView
  owns QGraphicsScene
  displays BaseComponent items
  displays Wire items

BaseComponent
  owns or references ConnectionPad entries
  stores editable properties
  stores live simulation state

Wire
  references start ConnectionPad
  references end ConnectionPad
  stores routed bend points

SimulationLoop
  owns QTimer
  calls domain solvers each tick (planned)

UndoRedoStack
  owns QUndoStack
  stores QUndoCommand instances
```

## Signal and Slot Relationships

Current:

```text
Toolbar/Menu Play  -> SimulationLoop::start()
Toolbar/Menu Pause -> SimulationLoop::pause()
Toolbar/Menu Reset -> SimulationLoop::reset()
QTimer::timeout    -> SimulationLoop::tick()
```

Planned:

```text
BaseComponent::propertyChanged
  -> PropertiesPanel refresh
  -> Scene dirty flag update
  -> Solver domain invalidation

SimulationLoop::tickComplete
  -> Graph widgets update
  -> Status bar time display
  -> Canvas repaint

UndoRedoStack state signals
  -> Edit menu Undo/Redo enabled state
```

## User Interaction Flow

```text
User drags part from PartsPanel
  -> PartsPanel creates QMimeData with typeId
  -> CanvasView receives drop event
  -> ComponentRegistry creates BaseComponent subclass
  -> InteractionEngine creates AddPartCommand
  -> UndoRedoStack pushes command
  -> QGraphicsScene displays component
  -> SimulationLoop includes component on next tick
```

## Wire Creation Flow

```text
User hovers component
  -> compatible ConnectionPad objects highlight
User clicks source pad
  -> wire drawing mode starts
User clicks canvas
  -> bend points are added
User clicks compatible target pad
  -> AddWireCommand creates Wire
  -> pads record the connection
  -> electronics topology marked dirty
  -> next solver tick rebuilds circuit graph
```

## Undo Example

```text
AddPartCommand::redo()
  -> add component to scene
  -> register component ID
  -> mark domain dirty

AddPartCommand::undo()
  -> remove component from scene
  -> detach wires
  -> preserve serialized component data for redo
```

## Design Notes

- `BaseComponent` currently combines graphics and simulation hooks for early velocity. If solver logic grows too heavy, extract domain behavior objects while preserving the same public component registry.
- `Wire` references pads but should not own components.
- Future scene ownership must be explicit before complex delete/undo behavior lands.

