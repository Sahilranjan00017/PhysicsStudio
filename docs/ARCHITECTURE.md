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
- `typeId`: registry key such as `ELEC_RES` or `MOT_BALL`.
- `displayName`: user-facing label like `R1`.
- `properties`: editable configuration values.
- `simState`: live values produced by solvers.
- `pads`: connection endpoints for wires and domain links.
- lock/destruction flags: used by classroom models and overload handling.

Each domain hook is deliberately no-op in the base class:

- `stampMNA(double dt)`: electronics components contribute to circuit equations.
- `stepMotion(MotionContext&, double dt)`: motion components apply forces or constraints.
- `traceRays(RayContext&)`: optical components emit or transform rays.
- `stepWave(WaveContext&, double dt)`: wave components update sources, boundaries, or media.

Example component behavior:

```cpp
class ResistorComponent final : public BaseComponent {
public:
    void stampMNA(double dt) override;
};
```

A resistor will read `properties["resistance"]`, look up the electrical node IDs assigned to its two pads, and stamp conductance into the electronics matrix.

```cpp
class BallComponent final : public BaseComponent {
public:
    void stepMotion(MotionContext& context, double dt) override;
};
```

A ball will read mass, radius, position, velocity, and material settings, then let the motion solver integrate forces and collisions.

```cpp
class MirrorComponent final : public BaseComponent {
public:
    void traceRays(RayContext& context) override;
};
```

A mirror will expose ray intersection geometry and reflection behavior inside an `OpticalSpace`.

```cpp
class WaveSourceComponent final : public BaseComponent {
public:
    void stepWave(WaveContext& context, double dt) override;
};
```

A wave source will inject amplitude into a 1D or 2D wave grid each tick.

## Solver Architecture

All solvers share an explicit interface in `src/simulation/SolverInterfaces.h`. The main simulation loop should not know solver internals; it should collect domain containers from the scene and call the proper solver.

Current planned interfaces:

- `IElectronicsSolver::solve(ElectronicsDomain&, double dt)`
- `IMotionSolver::step(MotionDomain&, double dt)`
- `IOpticsSolver::trace(OpticalDomain&)`
- `IWaveSolver::step(WaveDomain&, double dt)`

Electronics will use Modified Nodal Analysis. The expected shape is:

1. Build or reuse a `CircuitGraph` from electrical pads and wires.
2. Assign node IDs, including ground.
3. Allocate MNA matrix and right-hand side vector.
4. Ask each electronics component to stamp its contribution.
5. Solve the matrix.
6. Write node voltages and branch currents back into component `simState`.
7. Run overload detection.

Motion, optics, and waves follow the same orchestration pattern but with domain-specific containers:

- Motion: collect bodies, forces, constraints, collisions.
- Optics: trace dirty optical spaces only when geometry changes.
- Waves: step active wave grids every frame.

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

