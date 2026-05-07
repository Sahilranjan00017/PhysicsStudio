# Architecture

Physics Simulation Studio follows the module map from the TechDoc and keeps the system split into portable Qt/C++ layers. The target runtime is Windows 10/11, but the source tree is intentionally platform-neutral until packaging and validation.

## Goals

- Keep rendering, user interaction, simulation, and persistence separate.
- Allow each physics domain to grow without forcing every component to know about every solver.
- Keep the first Windows build simple: one Qt Widgets executable with clear source ownership.
- Make Phase 1 work additive: parts panel, registry, commands, and save/load should build on the current skeleton.

## Layers

- Application Core: startup, main window, settings, global command wiring.
- Canvas Engine: `QGraphicsScene` / `QGraphicsView`, grid, zoom, pan, canvas items.
- Interaction Engine: drag/drop, wire drawing, selection, undo/redo commands.
- Component System: shared component abstraction and domain-specific parts.
- Simulation Engine: 60 FPS tick loop and domain solver orchestration.
- Persistence: `.pss` JSON save/load, schema versioning, migration.
- Presentation: graphs, numbers, sliders, labels, scene navigation.
- Packaging: Windows deployment, `windeployqt`, installer scripts.

## Component System Design

`BaseComponent` is the root of all placeable simulation parts. It inherits from `QGraphicsObject` so every component can live directly on the Qt canvas, receive selection/move events later, and emit Qt signals for UI updates.

The common data model is:

- `id`: stable UUID used by persistence and wires.
- `typeId`: registry key such as `Resistor` or `Ball`.
- `displayName`: user-facing label like `R1`.
- `properties`: editable configuration values (QMap<QString, QVariant>).
- `simState`: live values produced by solvers.
- `pads`: connection endpoints for wires and domain links.
- lock/destruction flags: used by classroom models and overload handling.

### BaseComponent Class Hierarchy

```
QGraphicsObject (Qt framework)
└── BaseComponent (physics abstraction)
    ├── Electronics Domain
    │   ├── Resistor (fixed R)
    │   ├── Capacitor (dynamic C)
    │   ├── Inductor (dynamic L)
    │   ├── VoltageSource (constraint)
    │   ├── CurrentSource (source)
    │   ├── Ammeter (measurement)
    │   └── Voltmeter (measurement)
    ├── Motion Domain
    │   ├── Ball (rigid sphere)
    │   ├── Block (rigid box)
    │   ├── Spring (elastic link)
    │   └── Hinge (joint)
    ├── Optics Domain
    │   ├── Mirror (reflection)
    │   ├── Lens (refraction)
    │   ├── Prism (dispersion)
    │   └── Beamsplitter (transmission/reflection)
    └── Wave Domain
        ├── WaveSource (drives wave)
        ├── WaveDetector (measures wave)
        ├── WaveBoundary (reflection)
        └── WaveMaterial (absorption/speed)
```

### Virtual Method Pattern

Each domain hook is deliberately no-op in the base class. Subclasses override only the methods relevant to their physics domain:

- `stampMNA(double dt)`: electronics components contribute to circuit equations (MNA matrix).
- `stepMotion(MotionContext&, double dt)`: motion components apply forces or constraints (RK4 integration).
- `traceRays(RayContext&)`: optical components emit or transform rays (geometric ray tracing).
- `stepWave(WaveContext&, double dt)`: wave components update sources, boundaries, or media (wave equation).

### Component Examples

**Resistor (Electronics Only)**

```cpp
class Resistor final : public BaseComponent {
public:
    void stampMNA(double dt) override;  // Contributes to MNA matrix
    
    // Empty overrides for unused domains:
    void stepMotion(MotionContext&, double) override {}
    void traceRays(RayContext&) override {}
    void stepWave(WaveContext&, double) override {}
};
```

A resistor will read `properties["resistance"]`, look up the electrical node IDs assigned to its two pads, and stamp conductance into the electronics matrix.

**Ball (Motion Only)**

```cpp
class Ball final : public BaseComponent {
public:
    void stepMotion(MotionContext& context, double dt) override;  // Integrates dynamics
    
    // Empty overrides for unused domains:
    void stampMNA(double) override {}
    void traceRays(RayContext&) override {}
    void stepWave(WaveContext&, double) override {}
};
```

A ball will read mass, radius, position, velocity, and material settings, then let the motion solver integrate forces and collisions using RK4 numerical integration.

**Mirror (Optics Only)**

```cpp
class Mirror final : public BaseComponent {
public:
    void traceRays(RayContext& context) override;  // Reflects rays
    
    // Empty overrides for unused domains:
    void stampMNA(double) override {}
    void stepMotion(MotionContext&, double) override {}
    void stepWave(WaveContext&, double) override {}
};
```

A mirror will expose ray intersection geometry and reflection behavior inside an `OpticalSpace`.

**WaveSource (Wave Only)**

```cpp
class WaveSource final : public BaseComponent {
public:
    void stepWave(WaveContext& context, double dt) override;  // Drives oscillation
    
    // Empty overrides for unused domains:
    void stampMNA(double) override {}
    void stepMotion(MotionContext&, double) override {}
    void traceRays(RayContext&) override {}
};
```

A wave source will inject amplitude into a 1D or 2D wave grid each tick according to `properties["frequency"]` and `properties["amplitude"]`.

### ConnectionPads

Each component exposes connection points via `pads` (list of `ConnectionPad` structs):

```cpp
struct ConnectionPad {
    QPointF localPos;           // Position relative to component
    PadType type;               // Input / Output / Bidirectional
    DomainType domain;          // Electrical / Mechanical / Optical / Wave
    QString padId;              // Unique identifier
    QList<Wire*> connectedWires;// Attached connections
    bool highlighted;           // UI state
};
```

- **Electrical pads** connect via wires (share node voltage)
- **Mechanical pads** connect via springs/hinges (shared positions)
- **Optical pads** define ray entry/exit points
- **Wave pads** define propagation boundaries

### Serialization

All components inherit JSON serialization:

```cpp
virtual QJsonObject toJson() const;        // Save to JSON
virtual void fromJson(const QJsonObject&); // Load from JSON
```

`ProjectDocument` uses these methods to persist component trees with all properties and positions.

## Solver Architecture

All solvers are independent and decoupled. Each operates on its own domain container and updates only its components. Multiple domains can coexist in the same scene without interaction (until multi-domain features like piezoelectric effects are added in later phases).

### Domain-Specific Solvers

All solvers share an explicit interface in `src/simulation/SolverInterfaces.h`. The simulation loop should not know solver internals; it should collect domain containers from the scene and call the proper solver at each time step.

Solver interfaces:

- `IElectronicsSolver::solve(ElectronicsDomain&, double dt)` — Solves circuit equations via MNA
- `IMotionSolver::step(MotionDomain&, double dt)` — Integrates rigid body dynamics via RK4
- `IOpticsSolver::trace(OpticalDomain&)` — Traces rays through optical geometry
- `IWaveSolver::step(WaveDomain&, double dt)` — Steps wave equation on grids

### SimulationLoop Execution Pattern

The `SimulationLoop` class drives all solvers at 60 FPS:

```cpp
SimulationLoop (QObject):
  timer.setInterval(16);           // 16ms = 60 FPS
  
  On tick():
    if (running):
      dt = 0.016 * speedMultiplier
      
      electronics_solver.solve(domain, dt)
      motion_solver.step(domain, dt)
      optics_solver.trace(domain)
      wave_solver.step(domain, dt)
      
      canvas.update()
      emit tickComplete(simulationTime)
```

Execution order:
1. **Electronics** first — produces voltages and currents used by other domains
2. **Motion** second — applies electromagnetic forces, integrates positions
3. **Optics** third — traces rays through updated geometry
4. **Waves** last — updates wave fields (may be affected by motion)

### Electronics Solver: Modified Nodal Analysis (MNA)

Electronics uses **Modified Nodal Analysis** to solve the circuit:

1. Build a node list from electrical pads and wires across all components
2. Assign node indices (0, 1, 2, ..., n-1)
3. Allocate G matrix (n×n) and I vector (n)
4. For each electronic component, call `component->stampMNA(dt)`:
   - **Resistor**: Stamps conductance into G[i][j]
   - **VoltageSource**: Adds constraint equation (row in augmented matrix)
   - **CurrentSource**: Stamps current into I vector
   - **Capacitor**: Uses companion model (dynamic element via RK4)
5. Solve: **V = G^(-1) * I**
6. Update component `simState` with node voltages and wire currents

Electronics uses **Eigen3** library for dense matrix operations.

### Motion Solver: Runge-Kutta 4 (RK4) Integration

Motion uses numerical integration to advance rigid body dynamics:

1. Collect all bodies (components with `stepMotion()`)
2. Calculate total forces:
   - Gravity: F_g = mass * gravity vector
   - Springs: F_s = -k * (displacement) - d * (velocity)
   - Collisions: F_c = contact forces (impulse-based)
3. For each body, run RK4 integration step with acceleration = F/m:
   - Advances velocity and position by dt
   - Preserves energy and momentum
4. Check collisions and apply constraint forces
5. Update component positions on canvas

Motion tracks `simState["position"]`, `simState["velocity"]`, `simState["angularVelocity"]`.

### Optics Solver: Geometric Ray Tracing

Optics traces rays through optical elements:

1. For each active ray:
   - Track position and direction through space
   - Check intersection with optical surfaces
   - For each intersection, call `component->traceRays(context)`:
     - **Mirror**: Reflects ray (inverts normal component)
     - **Lens**: Refracts ray (applies Snell's law)
     - **Prism**: Refracts with wavelength-dependent refraction index
2. Accumulate ray contributions to optical targets
3. Display traced rays as visual feedback

Optics is purely geometric (no equations to solve).

### Wave Solver: Finite Difference Time Domain (FDTD)

Wave solves the discrete wave equation on a 1D or 2D grid:

1. Allocate field grid (complex values for amplitude + phase)
2. For each time step:
   - Apply finite differences to interior points: u_new = 2u - u_old + c²*dt² * laplacian(u)
   - Apply boundary conditions (walls, absorbers, materials)
   - Call `component->stepWave(context, dt)`:
     - **WaveSource**: Injects amplitude at source position
     - **WaveBoundary**: Applies reflection/absorption
     - **WaveMaterial**: Modifies wave speed
3. Display field as heatmap or contour plot

Wave uses compact storage (only current + previous frame).

## Undo/Redo System

Canvas mutations must be represented as commands, not direct one-off edits. `UndoRedoStack` wraps `QUndoStack` and will own commands such as:

- `AddPartCommand`
- `DeletePartCommand`
- `MovePartCommand`
- `RotatePartCommand`
- `AddWireCommand`
- `ChangePropertyCommand`

Example flow for adding a component:

1. Parts panel starts a drag with component `typeId`.
2. Canvas drop handler asks the component registry to create the part.
3. Interaction engine creates `AddPartCommand`.
4. `UndoRedoStack::push()` executes and stores the command.
5. Undo removes the component and any transient links.
6. Redo restores the same component ID and properties.

## Persistence

The project file extension is `.pss`. The format is JSON and should remain human-readable. `ProjectDocument` currently stores the schema version and will expand to hold scenes, components, wires, presentation objects, and scene links.

Persistence rules:

- Store stable IDs, not raw pointers.
- Store properties separately from live simulation state.
- Keep schema version at the root as `pss_version`.
- Add migrations when older files are loaded.

## Dependency Direction

Dependencies should flow inward from orchestration to stable domain contracts:

```text
MainWindow
  -> CanvasView
  -> UndoRedoStack
  -> SimulationLoop
       -> Solver interfaces
  -> ProjectDocument

CanvasView
  -> BaseComponent
  -> Wire
  -> ConnectionPad

Domain components
  -> BaseComponent
  -> domain contexts
```

Persistence must not depend on UI widgets. Solvers must not depend on dock widgets, menus, or toolbars. Components may expose Qt graphics behavior, but solver math should be kept in domain classes where practical.

## Phase Boundaries

Phase 0.1a is portable foundation only. Phase 0.1b must validate the same code on Windows with MSVC and Qt6. Phase 0.2 should not begin until the Windows build and launch checks pass.

