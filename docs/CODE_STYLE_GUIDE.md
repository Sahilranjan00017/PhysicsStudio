# Code Style Guide

This guide captures the conventions used by the Phase 0.1a foundation. Keep new code consistent unless a stronger local Qt convention applies.

## Language and Build

- Use C++20.
- Use Qt6 APIs for UI, signals, widgets, and graphics items.
- Prefer portable C++ and Qt behavior until Windows-specific packaging.
- Keep compiler warnings clean under MSVC `/W4` and GCC/Clang `-Wall -Wextra -Wpedantic`.

## Naming

- Classes and structs: `PascalCase`
  - `BaseComponent`
  - `CanvasView`
  - `SimulationLoop`
- Methods and functions: `camelCase`
  - `stampMNA`
  - `stepMotion`
  - `toJson`
- Variables and members: `camelCase`
  - `simulationTime`
  - `fixedFrameDt`
  - `canvasView`
- Enum classes: `PascalCase` for type, `PascalCase` values
  - `enum class WireType { Power, Ground, Signal }`
- Constants: prefer descriptive `camelCase` or `PascalCase` scoped constants. Avoid preprocessor constants.

Private members may use plain `camelCase` in small classes. If a class grows large enough that member/local ambiguity becomes a problem, introduce a consistent `m_` prefix in that class as part of a focused refactor.

## Formatting

- Indentation: 4 spaces.
- Braces: opening brace on the same line for classes, functions, and control blocks.
- Prefer one declaration per line when declarations have initializers.
- Keep includes grouped from local to Qt/standard:

```cpp
#include "app/MainWindow.h"

#include "canvas/CanvasView.h"

#include <QAction>
#include <QMenuBar>
```

## Header Rules

- Use `#pragma once`.
- Forward declare Qt classes when possible in headers.
- Include full definitions in `.cpp` files.
- Keep headers small and stable; avoid exposing implementation-only dependencies.

## Qt Conventions

- Classes with signals/slots must include `Q_OBJECT`.
- Use QObject parent ownership for widgets and timers where practical.
- Use `override` for overridden virtual methods.
- Mark leaf classes `final` when extension is not expected.
- Prefer signal/slot connections over manual polling for UI state changes.

## Memory Ownership

- Qt widgets and QObject children should usually be parent-owned.
- Avoid raw owning pointers outside Qt parent ownership.
- Non-owning pointers are acceptable for scene links, pads, and wires, but ownership must be documented when introduced.
- Persistence must store IDs, not raw pointers.

## Comments

- Use comments sparingly.
- Explain intent, tradeoffs, or non-obvious behavior.
- Avoid comments that restate the code.

Good:

```cpp
// MNA matrix assembly will land after component stamping contracts are fixed.
```

Avoid:

```cpp
// Increment i by one.
```

## Error Handling

- UI-facing errors should become clear messages in the status bar, dialog, or validation report.
- File loading should validate schema version and fail with actionable messages.
- Solver failures should not crash the app; record the error in domain state and notify the UI.

## Git Discipline

- Keep commits small and phase-oriented.
- Do not mix formatting churn with behavior changes.
- Commit generated planning docs separately from source changes when possible.

