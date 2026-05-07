# Interaction Flow & Command Pattern

## Overview

Physics Simulation Studio uses the **Command Pattern** to implement undo/redo functionality and to coordinate user actions across the UI. Every user action (create component, move component, edit property, delete component) is encapsulated in a `QUndoCommand` subclass and executed through the `UndoRedoStack`.

This architecture ensures:
- Every action is undoable
- Actions can be queued and executed atomically
- No special-case undo logic needed
- Action history is implicit (stack of commands)

---

## System Architecture

```
MainWindow
├── CanvasView (central widget)
│   └── QGraphicsScene (holds components and wires)
├── UndoRedoStack (command history)
├── Parts Panel (left dock)
├── Properties Panel (right dock)
└── SimulationLoop (60 FPS simulation)
```

### Component Roles

**MainWindow**: Coordinator
- Creates and owns CanvasView, UndoRedoStack, SimulationLoop
- Routes menu/toolbar actions to simulationLoop
- Creates dock panels for Parts and Properties

**CanvasView**: Canvas Widget
- Inherits from QGraphicsView
- Contains QGraphicsScene for rendering and interaction
- Draws grid background (20 pixel grid)
- Handles drop events for component creation
- Handles selection, drag, resize operations

**UndoRedoStack**: Command Manager
- Wrapper around Qt's `QUndoStack`
- Public API: `push()`, `undo()`, `redo()`, `canUndo()`, `canRedo()`
- Commands are pushed as user acts; stack manages undo/redo

**Parts Panel**: Component Catalog
- Lists available component types (Resistor, Ball, Mirror, WaveSource, etc.)
- User drags from this panel onto the canvas

**Properties Panel**: Component Editor
- Shows properties of selected component
- User edits values here
- Triggers `propertyChanged` signal on component
- EditorCommand captures the change for undo/redo

---

## User Action Flow: Drop Component

This is the most fundamental interaction—the user drags a component type from Parts Panel and drops it on the canvas.

```
┌─────────────────────────────────────────────────────┐
│ USER ACTION: Drag "Resistor" from Parts → Canvas   │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│ Parts Panel (QDockWidget)                           │
│ ├─ ComponentLibrary (lists all types)              │
│ └─ startDrag("Resistor") [MIME type: component]    │
└─────────────────────────────────────────────────────┘
                          ↓
    [Drag in progress - user moves mouse over canvas]
                          ↓
┌─────────────────────────────────────────────────────┐
│ CanvasView                                          │
│ └─ dragEnterEvent() → accept if MIME type matches  │
└─────────────────────────────────────────────────────┘
                          ↓
    [User releases mouse button]
                          ↓
┌─────────────────────────────────────────────────────┐
│ CanvasView::dropEvent(drop position, MIME data)    │
│                                                    │
│ 1. Extract component type from MIME data           │
│    → "Resistor"                                    │
│                                                    │
│ 2. Extract drop position from event               │
│    → QPointF(200.0, 150.0)                        │
│                                                    │
│ 3. Create CreateComponentCommand                  │
│    → new CreateComponentCommand(                  │
│        "Resistor",                                │
│        QPointF(200, 150),                         │
│        scene)                                     │
│                                                    │
│ 4. Push command to UndoRedoStack                  │
│    → undoRedoStack->push(command)                 │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│ UndoRedoStack::push(command)                       │
│ └─ stack.push(command)   [Qt's QUndoStack]        │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│ CreateComponentCommand::redo() [called by Qt]      │
│ (undo/redo use same virtual method pattern)       │
│                                                    │
│ 1. Create component via ComponentFactory          │
│    → Resistor* resistor = ComponentFactory::create("Resistor")
│                                                    │
│ 2. Set component position                         │
│    → resistor->setPos(200.0, 150.0)               │
│                                                    │
│ 3. Assign unique ID                               │
│    → resistor->id = generateUUID()                │
│                                                    │
│ 4. Add to scene                                   │
│    → scene->addItem(resistor)                     │
│                                                    │
│ 5. Connect signals                                │
│    → connect(resistor, &BaseComponent::propertyChanged,
│           propertiesPanel, &PropertiesPanel::onPropertyChanged)
│                                                    │
│ 6. Emit componentAdded signal                     │
│    → emit componentAdded(resistor)                │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│ CanvasView (QGraphicsScene)                        │
│ └─ scene->itemAdded(resistor)                     │
│    └─ Canvas repaints                            │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│ USER SEES: Resistor appears at drop position      │
└─────────────────────────────────────────────────────┘
```

---

## Command Pattern: Virtual Method Pair

Each command subclass implements two methods (Qt's `QUndoCommand` interface):

```cpp
class CreateComponentCommand : public QUndoCommand {
public:
    void redo() override {
        // Do the action (create component)
        // Called when command is first pushed to stack
    }
    
    void undo() override {
        // Reverse the action (delete component)
        // Called when user presses Ctrl+Z
    }
};
```

The pattern ensures symmetry:
- `redo()` performs the action
- `undo()` reverses it completely
- No state corruption possible

---

## Standard Commands

### CreateComponentCommand

**What**: Creates a component on the canvas

**redo()**:
```cpp
component = ComponentFactory::create(typeId);
component->setPos(position);
component->id = UUID::generate();
scene->addItem(component);
```

**undo()**:
```cpp
scene->removeItem(component);
delete component;
```

---

### MoveComponentCommand

**What**: Moves a component from one position to another

**Constructor**:
```cpp
MoveComponentCommand(BaseComponent* comp, QPointF oldPos, QPointF newPos)
```

**redo()**:
```cpp
component->setPos(newPos);
```

**undo()**:
```cpp
component->setPos(oldPos);
```

**Optimization**: If user drags component continuously, multiple MoveComponentCommand instances might merge:
```cpp
bool MoveComponentCommand::mergeWith(const QUndoCommand* other) {
    auto* moveCmd = dynamic_cast<const MoveComponentCommand*>(other);
    if (!moveCmd || moveCmd->component != component) return false;
    
    newPos = moveCmd->newPos;  // Update final position only
    return true;  // Merge successful - don't push new command
}
```

This way, dragging 100 pixels creates 1 command (final position), not 100 commands.

---

### EditPropertyCommand

**What**: Changes a component's property (e.g., resistance value)

**Constructor**:
```cpp
EditPropertyCommand(BaseComponent* comp, QString key, QVariant oldValue, QVariant newValue)
```

**redo()**:
```cpp
component->properties[key] = newValue;
emit component->propertyChanged(key, newValue);
```

**undo()**:
```cpp
component->properties[key] = oldValue;
emit component->propertyChanged(key, oldValue);
```

---

### DeleteComponentCommand

**What**: Removes a component from canvas (stores for undo)

**Constructor**:
```cpp
DeleteComponentCommand(BaseComponent* comp, QGraphicsScene* scene)
```

**redo()**:
```cpp
scene->removeItem(component);
// Note: Don't delete yet—store for undo()
```

**undo()**:
```cpp
scene->addItem(component);
// Component is restored
```

---

### ConnectWireCommand

**What**: Creates a wire between two pads

**Constructor**:
```cpp
ConnectWireCommand(ConnectionPad* startPad, ConnectionPad* endPad, QGraphicsScene* scene)
```

**redo()**:
```cpp
wire = new Wire();
wire->startPad = startPad;
wire->endPad = endPad;
startPad->connectedWires.append(wire);
endPad->connectedWires.append(wire);
scene->addItem(wire);
```

**undo()**:
```cpp
scene->removeItem(wire);
startPad->connectedWires.removeAll(wire);
endPad->connectedWires.removeAll(wire);
delete wire;
```

---

## User Action Sequences

### Action: Undo (Ctrl+Z)

```
User presses Ctrl+Z
    ↓
MainWindow slot: onUndo()
    ↓
UndoRedoStack::undo()
    ↓
QUndoStack::undo()
    ↓
Current command->undo() [virtual]
    ↓
Component is destroyed/moved/edited (reversed)
    ↓
Canvas repaints
    ↓
User sees previous state restored
```

### Action: Redo (Ctrl+Y or Ctrl+Shift+Z)

Same as undo, but calls `redo()` instead of `undo()`.

---

## Property Edit Flow

When user edits a property in the Properties Panel:

```
┌──────────────────────────────────┐
│ Properties Panel                 │
│ ├─ Resistance slider [1000Ω]    │
│ └─ User moves slider to 2000Ω   │
└──────────────────────────────────┘
            ↓
┌──────────────────────────────────┐
│ PropertiesPanel::onResistanceChanged(2000)
│ └─ Create EditPropertyCommand    │
│    - component = selected        │
│    - key = "resistance"          │
│    - oldValue = 1000             │
│    - newValue = 2000             │
└──────────────────────────────────┘
            ↓
┌──────────────────────────────────┐
│ UndoRedoStack::push(command)    │
│ └─ command->redo() executed      │
│    └─ component->properties["resistance"] = 2000
│    └─ emit propertyChanged()     │
└──────────────────────────────────┘
            ↓
┌──────────────────────────────────┐
│ SimulationLoop receives signal   │
│ └─ Updates solver with new R     │
└──────────────────────────────────┘
            ↓
┌──────────────────────────────────┐
│ User sees circuit behavior change│
│ (different voltage divider ratio)│
└──────────────────────────────────┘
```

If user undoes:
```
User presses Ctrl+Z
    ↓
EditPropertyCommand::undo()
    ↓
component->properties["resistance"] = 1000
    ↓
emit propertyChanged()
    ↓
SimulationLoop updates solver
    ↓
Behavior reverts
```

---

## Selection & Highlighting

When user clicks a component:

```
Canvas receives mousePressEvent
    ↓
If item is BaseComponent:
    ├─ currentSelection = item
    ├─ item->setSelected(true)
    └─ emit selectionChanged(item)
            ↓
PropertiesPanel receives signal
    ├─ Clear old property widgets
    ├─ Load item->properties
    └─ Display editable UI for each property
```

When user clicks empty space:

```
Canvas receives mousePressEvent on empty area
    ↓
currentSelection = nullptr
    ↓
emit selectionChanged(nullptr)
    ↓
PropertiesPanel clears
```

---

## Undo/Redo Stack Behavior

### After Create, Move, Edit:

```
undo stack:  [CreateComponentCommand]
redo stack:  []

User presses Ctrl+Z:
undo stack:  []
redo stack:  [CreateComponentCommand]
Canvas:      Component removed

User presses Ctrl+Y:
undo stack:  [CreateComponentCommand]
redo stack:  []
Canvas:      Component restored
```

### Command Merging (Optimization)

If user drags a component 10 pixels with mouse:

Without merging:
```
undo stack:  [MoveCmd(0→1px), MoveCmd(1→2px), ..., MoveCmd(9→10px)]
Number of commands: 10
```

With merging (via `mergeWith()`):
```
undo stack:  [MoveCmd(0→10px)]
Number of commands: 1
```

This reduces memory and simplifies undo history.

---

## Ready for Phase 1 Implementation

This architecture is ready for concrete command implementation. Phase 1 will:

1. **Implement ComponentFactory** to create components by typeId
2. **Create Command Classes**:
   - CreateComponentCommand
   - DeleteComponentCommand
   - MoveComponentCommand (with merging)
   - EditPropertyCommand
   - ConnectWireCommand
3. **Connect Canvas to Commands**:
   - dropEvent → CreateComponentCommand
   - mouseMoveEvent → MoveComponentCommand
   - keyPressEvent(Delete) → DeleteComponentCommand
4. **Connect Properties Panel**:
   - Property slider/text → EditPropertyCommand

No changes to this design needed. The pattern scales smoothly through all phases.
