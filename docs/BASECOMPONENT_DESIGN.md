# BaseComponent Architecture Design

## Overview

The `BaseComponent` class is the foundational architecture for all simulation components in Physics Simulation Studio. It uses the **Virtual Method Pattern** to enable different physics domains (Electronics, Motion, Optics, Waves) to coexist in the same component hierarchy while remaining completely decoupled.

---

## Class Hierarchy

```
QGraphicsObject (Qt base class for graphics items)
│
└── BaseComponent (abstract physics component)
    ├── Electronics Domain
    │   ├── Resistor
    │   ├── Capacitor
    │   ├── Inductor
    │   ├── VoltageSource
    │   ├── CurrentSource
    │   ├── Ammeter
    │   └── Voltmeter
    │
    ├── Motion Domain
    │   ├── Ball
    │   ├── Block
    │   ├── Spring
    │   └── Hinge
    │
    ├── Optics Domain
    │   ├── Mirror
    │   ├── Lens
    │   ├── Prism
    │   └── Beamsplitter
    │
    └── Wave Domain
        ├── WaveSource
        ├── WaveDetector
        ├── WaveBoundary
        └── WaveMaterial
```

---

## Virtual Method Pattern

Each `BaseComponent` subclass implements up to **4 domain-specific virtual methods**. Only implement the methods relevant to that component's domain.

### The Four Virtual Methods

**1. `stampMNA(double dt)` — Electronics Domain**
- Stamps component's contribution to Modified Nodal Analysis matrix
- Used by ElectronicsSolver to build circuit equations
- Default implementation: empty (no-op)
- Example: `Resistor::stampMNA()` adds conductance to G matrix

**2. `stepMotion(MotionContext& context, double dt)` — Motion Domain**
- Advances motion simulation by one time step
- Updates velocity and position via integration (RK4)
- Default implementation: empty (no-op)
- Example: `Ball::stepMotion()` updates position based on velocity, applies forces

**3. `traceRays(RayContext& context)` — Optics Domain**
- Processes ray-optical interactions with this component
- Traces rays through or bounces them off geometry
- Default implementation: empty (no-op)
- Example: `Mirror::traceRays()` reflects incoming rays

**4. `stepWave(WaveContext& context, double dt)` — Wave Domain**
- Updates wave field values for this component
- Drives wave sources, applies boundary conditions
- Default implementation: empty (no-op)
- Example: `WaveSource::stepWave()` drives sinusoidal oscillation

### Implementation Pattern

```cpp
// Example: Resistor (Electronics only)
class Resistor : public BaseComponent {
public:
    void stampMNA(double dt) override;
    void stepMotion(MotionContext&, double) override {}      // Empty
    void traceRays(RayContext&) override {}                  // Empty
    void stepWave(WaveContext&, double) override {}          // Empty
};

// Example: Ball (Motion only)
class Ball : public BaseComponent {
public:
    void stampMNA(double) override {}                        // Empty
    void stepMotion(MotionContext& c, double dt) override;
    void traceRays(RayContext&) override {}                  // Empty
    void stepWave(WaveContext&, double) override {}          // Empty
};

// Example: Mirror (Optics only)
class Mirror : public BaseComponent {
public:
    void stampMNA(double) override {}                        // Empty
    void stepMotion(MotionContext&, double) override {}      // Empty
    void traceRays(RayContext& c) override;
    void stepWave(WaveContext&, double) override {}          // Empty
};
```

---

## Core Data Members

### Identification & Metadata

```cpp
QString id;                    // Unique UUID for this component
QString typeId;                // "Resistor", "Ball", "Mirror", etc.
QString displayName;           // User-visible name (e.g., "R1", "Ball-3")
```

### Graphical & Physical Positioning

```cpp
double rotationDegrees;        // Component rotation (0-360)
bool lockedPosition;           // If true, user cannot drag component
bool lockedSize;               // If true, user cannot resize
bool lockedOrientation;        // If true, user cannot rotate
```

### Lifecycle & State

```cpp
bool destructible;             // If true, can be deleted
bool destroyed;                // If true, component is destroyed (but not deleted)
bool lockedProperties;         // If true, properties cannot be edited
```

### Domain Data

```cpp
QMap<QString, QVariant> properties;    // Domain-specific properties
                                       // Example: {"resistance": 1000.0, "label": "R1"}

QMap<QString, QVariant> simState;      // Runtime simulation state
                                       // Example: {"voltage": 5.0, "current": 0.005}

QList<ConnectionPad*> pads;            // Connection points for wires
```

---

## Connection Model

### ConnectionPad Structure

Each component has a list of `ConnectionPad` structs. Pads are the connection points where wires can attach.

```cpp
struct ConnectionPad {
    QPointF localPos;                  // Position relative to component (local coords)
    PadType type;                      // Input, Output, or Bidirectional
    DomainType domain;                 // Electrical, Mechanical, Optical, or Wave
    QString padId;                     // Unique pad identifier
    QList<Wire*> connectedWires;       // Wires attached to this pad
    bool highlighted;                  // UI highlight state
};
```

### Pad Domain Types

- **Electrical**: For circuit connections (wires, voltage nodes)
- **Mechanical**: For mechanical connections (springs, hinges, contacts)
- **Optical**: For optical connections (ray paths)
- **Wave**: For wave propagation paths

### Wire Structure

Wires connect two pads together:

```cpp
class Wire final : public QGraphicsItem {
    QList<QPointF> points;             // Routing waypoints
    ConnectionPad* startPad;           // Origin pad
    ConnectionPad* endPad;             // Destination pad
    QVariant signal;                   // Propagated signal/value
    WireType wireType;                 // Power, Ground, Signal, Optical, Mechanical
};
```

---

## Serialization API

### Saving to JSON

```cpp
virtual QJsonObject toJson() const;
```

Converts component state to JSON. Handles:
- ID, typeId, displayName
- Position (x, y)
- Rotation
- Lock states
- Custom properties map

### Loading from JSON

```cpp
virtual void fromJson(const QJsonObject& object);
```

Restores component state from JSON. Called by ProjectDocument when loading files.

---

## Signal/Slot System

Inherits from `QObject` (via `QGraphicsObject`) for Qt signal/slot support.

### Signals

**`propertyChanged(QString key, QVariant value)`**
- Emitted when a property value changes
- Listener: UI panels, solvers that depend on the property
- Example: User changes resistance value → Resistor emits `propertyChanged("resistance", 2000)`

**`destroyedSignal(BaseComponent* component)`**
- Emitted when component is destroyed
- Listener: Canvas to remove component visuals
- Listener: Solvers to update their matrices

**`repairedSignal(BaseComponent* component)`**
- Emitted when component recovers from error state
- Listener: UI to show component as "healthy" again
- Example: Overload circuit recovers after transient voltage spike passes

---

## Example: Resistor Implementation

Here's what a concrete `Resistor` class would look like:

```cpp
// Resistor.h
#pragma once
#include "components/BaseComponent.h"

class Resistor : public BaseComponent {
    Q_OBJECT

public:
    explicit Resistor(QGraphicsItem* parent = nullptr);
    
    void stampMNA(double dt) override;  // Add to circuit matrix
    
    // Not used for electronics:
    void stepMotion(MotionContext&, double) override {}
    void traceRays(RayContext&) override {}
    void stepWave(WaveContext&, double) override {}

private:
    static constexpr const char* RESISTANCE_KEY = "resistance";
    static constexpr double DEFAULT_RESISTANCE = 1000.0;
};

// Resistor.cpp
#include "components/Resistor.h"
#include "simulation/electronics/ElectronicsSolver.h"

Resistor::Resistor(QGraphicsItem* parent)
    : BaseComponent(parent)
{
    typeId = "Resistor";
    displayName = "R1";
    
    // Set default properties
    properties[RESISTANCE_KEY] = DEFAULT_RESISTANCE;
    
    // Create pads (left and right terminals)
    auto leftPad = new ConnectionPad{
        QPointF(-20, 0),
        PadType::Bidirectional,
        DomainType::Electrical,
        "left"
    };
    auto rightPad = new ConnectionPad{
        QPointF(20, 0),
        PadType::Bidirectional,
        DomainType::Electrical,
        "right"
    };
    pads.append(leftPad);
    pads.append(rightPad);
}

void Resistor::stampMNA(double dt)
{
    double resistance = properties[RESISTANCE_KEY].toDouble();
    double conductance = 1.0 / resistance;
    
    // Stamp into MNA matrix
    // G[left_node][left_node] += conductance
    // G[left_node][right_node] -= conductance
    // G[right_node][left_node] -= conductance
    // G[right_node][right_node] += conductance
    
    // Details depend on node numbering (done by ElectronicsSolver)
}
```

---

## Integration with Solvers

### Electronics Domain

When `ElectronicsSolver` steps:

1. Iterates all BaseComponent instances
2. Calls `component->stampMNA(dt)`
3. Resistor adds conductance to MNA matrix
4. Capacitor adds dynamic element
5. Voltage source adds constraint equation
6. Solver assembles and solves system
7. Solver updates component `simState["voltage"]` and `simState["current"]`

### Motion Domain

When `MotionSolver` steps:

1. Iterates all BaseComponent instances
2. Calls `component->stepMotion(context, dt)`
3. Ball updates position based on velocity
4. Spring applies restoring force
5. Hinge constrains rotation
6. Solver updates `simState["position"]`, `simState["velocity"]`

---

## Key Design Decisions

### Why Virtual Methods?

- **Decoupling**: Each domain is independent. Adding Optics doesn't affect Electronics code.
- **Extensibility**: New domains just need new virtual method + context class.
- **Simplicity**: Clear contract for what each component does.

### Why QVariant Maps?

- **Flexibility**: Properties can be any Qt type (int, double, string, etc.)
- **Dynamic**: Properties defined at runtime, not compiled in
- **Serializable**: Maps convert to/from JSON trivially

### Why Pads Instead of Direct Wires?

- **Geometric**: Pads have position, allowing connection visualization
- **Type-Safe**: Domain type prevents crossing wires (electrical to optical)
- **Flexible**: Multiple pads enable multi-terminal components (3-phase, parallel paths)

### Why Context Classes?

Context classes (MotionContext, RayContext, WaveContext) bundle solver state so components can interact with the solver during simulation. This avoids passing 10+ parameters to stepMotion().

---

## Ready for Phase 1

This architecture is ready for component implementation. Phase 1 will:

1. Create ComponentFactory for instantiation
2. Implement Parts Panel UI for dragging components
3. Create CreateComponentCommand for undo/redo
4. Begin with Resistor, VoltageSource, Ammeter, Voltmeter
5. Stub Ball, Mirror, WaveSource (empty implementations until Phase 2)

No architectural changes needed. This foundation scales to all 4 domains.
