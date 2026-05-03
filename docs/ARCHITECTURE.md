# Architecture

Physics Simulation Studio follows the module map from the TechDoc.

## Layers

- Application Core: startup, main window, settings, command wiring
- Canvas Engine: scene/view, grid, zoom, pan, item rendering
- Interaction Engine: drag/drop, wire routing, selection, undo/redo
- Component System: shared component abstraction and domain-specific parts
- Simulation Engine: 60 FPS tick loop and domain solver orchestration
- Persistence: `.pss` JSON save/load and schema migration
- Presentation: graphs, numbers, sliders, labels, scene navigation

## Dependency Direction

UI and application classes own orchestration. Components expose stable simulation hooks. Solvers consume scene/domain state and write results back through dedicated state objects. Persistence serializes project data without depending on UI widgets.

