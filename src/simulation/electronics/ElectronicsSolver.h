#pragma once

#include "simulation/SolverInterfaces.h"

#include <QList>
#include <QString>

class BaseComponent;
class Wire;

// ---------------------------------------------------------------------------
// ElectronicsDomain
// Snapshot of the scene data the solver needs each tick.
// The CanvasView (or MainWindow) fills this before calling solve().
// ---------------------------------------------------------------------------
struct ElectronicsDomain {
    QList<BaseComponent*> components;   // all components with electrical pads
    QList<Wire*>          wires;        // all wires connecting those pads
    double simTime = 0.0;               // accumulated time (s) — used by AC source
};

// ---------------------------------------------------------------------------
// MnaResult
// Per-component read-back written into BaseComponent::simState after solve.
// Keys match what AmmeterComponent / VoltmeterComponent read for display.
// ---------------------------------------------------------------------------
struct MnaResult {
    double voltageA = 0.0;   // voltage at pad "a" (relative to ground)
    double voltageB = 0.0;   // voltage at pad "b" (relative to ground)
    double current  = 0.0;   // conventional current a→b through this branch
};

// ---------------------------------------------------------------------------
// ElectronicsSolver
// Implements IElectronicsSolver using Modified Nodal Analysis (MNA).
// Assembles G·x = b, solves via Eigen LU, writes results back to simState.
// ---------------------------------------------------------------------------
class ElectronicsSolver final : public IElectronicsSolver {
public:
    void solve(ElectronicsDomain& domain, double dt) override;

private:
    // Returns false and emits a qWarning if topology is invalid (no ground,
    // floating nodes, parallel voltage sources, etc.).
    bool validate(const ElectronicsDomain& domain) const;
};
